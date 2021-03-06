#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "linear.h"
#include "mpi.h"
const int BLOCK = 2;
const int N = 4;
const int sum_pro = BLOCK * BLOCK * N;


int print_null(const char *s,...) {return 0;}

static int (*info)(const char *fmt,...) = &printf;

struct feature_node *x;
int max_nr_attr = 64;

struct model* model_[sum_pro];
int flag_predict_probability=0;
// int sum_pro = BLOCK * BLOCK * 4;
double p_label[sum_pro];

void exit_input_error(int line_num)
{
	fprintf(stderr,"Wrong input format at line %d\n", line_num);
	exit(1);
}

static char *line = NULL;
static int max_line_len;

static char* readline(FILE *input)
{
	int len;

	if(fgets(line,max_line_len,input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line,max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}

void do_predict(FILE *input, FILE *output, int pid)
{
	int correct = 0;
	int total = 0;
	double error = 0;
	double sump = 0, sumt = 0, sumpp = 0, sumtt = 0, sumpt = 0;

	int nr_class=get_nr_class(model_[0]);
	double *prob_estimates=NULL;
	int j, n;
	int nr_feature=get_nr_feature(model_[0]);
	if(model_[0]->bias>=0)
		n=nr_feature+1;
	else
		n=nr_feature;

	if(flag_predict_probability)
	{
		int *labels;

		if(!check_probability_model(model_[0]))
		{
			fprintf(stderr, "probability output is only supported for logistic regression\n");
			exit(1);
		}

		labels=(int *) malloc(nr_class*sizeof(int));
		get_labels(model_[0],labels);
		prob_estimates = (double *) malloc(nr_class*sizeof(double));
		fprintf(output,"labels");
		for(j=0;j<nr_class;j++)
			fprintf(output," %d",labels[j]);
		fprintf(output,"\n");
		free(labels);
	}

	max_line_len = 1024;
	line = (char *)malloc(max_line_len*sizeof(char));
	while(readline(input) != NULL)
	{
		int i = 0;
		double target_label;
		int  predict_label;
		char *idx, *val, *label, *endptr;
		int inst_max_index = 0; // strtol gives 0 if wrong format

		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(total+1);

		// target_label = strtod(label,&endptr);
		if (label[0] == 'A') {
			target_label = 0;
		}
		else {
			target_label = 1;
		}
		// switch (label[0]) {
		// 	case 'A': target_label = 0; break;
		// 	case 'B': target_label = 1; break;
		// 	case 'C': target_label = 1; break;
		// 	case 'D': target_label = 1; break;
		// }
		// if(endptr == label || *endptr != '\0')
		// 	exit_input_error(total+1);
		for (int left = 0; left < BLOCK; left++) {
			while(1)
			{
				if(i>=max_nr_attr-2)	// need one more for index = -1
				{
					max_nr_attr *= 2;
					x = (struct feature_node *) realloc(x,max_nr_attr*sizeof(struct feature_node));
				}

				idx = strtok(NULL,":");
				val = strtok(NULL," \t");

				if(val == NULL)
					break;
				errno = 0;
				x[i].index = (int) strtol(idx,&endptr,10);
				if(endptr == idx || errno != 0 || *endptr != '\0' || x[i].index <= inst_max_index)
					exit_input_error(total+1);
				else
					inst_max_index = x[i].index;

				errno = 0;
				x[i].value = strtod(val,&endptr);
				if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
					exit_input_error(total+1);

				// feature indices larger than those in training are not used
				if(x[i].index <= nr_feature)
					++i;
			}

			if(model_[left]->bias>=0)
			{
				x[i].index = n;
				x[i].value = model_[pid]->bias;
				i++;
			}
			x[i].index = -1;

			if(flag_predict_probability)
			{
				int j;
				predict_label = predict_probability(model_[pid],x,prob_estimates);
				fprintf(output,"%g",predict_label);
				for(j=0;j<model_[left]->nr_class;j++)
					fprintf(output," %g",prob_estimates[j]);
				fprintf(output,"\n");
			}
			else
			{
				p_label[left] = (int)predict(model_[left],x);
				// printf("pid%dhas done\n",pid );
			}
		}
		// int count = 0;
		predict_label = 1;
		for ( int left = 0; left < BLOCK ; left++) {
			// for (int m = 0;m < BLOCK * N; m++) {
				// printf("%f\t", p_label[l * BLOCK + m]);
			predict_label &= (int)p_label[left];
			// if (predict_label == 1) {
			// 	predict_label = 0;
			// }
					// p_label[l] = 1;
					// break;
			}		
					// predict_label = 1;

		MPI_Allreduce(&predict_label, &predict_label, 1, MPI_INT, MPI_BOR,MPI_COMM_WORLD);
				
			// }
			// if (p_label[l] < 4) {
			// 	count++;
			// }
			// if ( p_label[l] == 1) {
			// 	predict_label = 1;
			// }
			// else {
			// 	predict_label = 0;
			// }
			// if (count >0) {
			// 	predict_label = 1;
			// }
			// else {
			// 	predict_label = 0;
			// }
		// }

		// if (count > 0 ) {
		// 	predict_label = 0;
		// 	}
		// else {
		// 	predict_label = 1;
		// }
		// /printf("\n");
		// fprintf(output,"%g\n",predict_label);

		if (pid == 0){
			// printf("%d\t", predict_label);
			if(predict_label == target_label)
				++correct;
		
			error += (predict_label-target_label)*(predict_label-target_label);
			sump += predict_label;
			sumt += target_label;
			sumpp += predict_label*predict_label;
			sumtt += target_label*target_label;
			sumpt += predict_label*target_label;
			++total;
			// printf("finish in %d\n", total);
		// }
	}


	} 
	if (pid == 0) {
		if(check_regression_model(model_[0]))
		{
			info("Mean squared error = %g (regression)\n",error/total);
			info("Squared correlation coefficient = %g (regression)\n",
				((total*sumpt-sump*sumt)*(total*sumpt-sump*sumt))/
				((total*sumpp-sump*sump)*(total*sumtt-sumt*sumt))
				);
		}
		else
			info("Accuracy = %g%% (%d/%d)\n",(double) correct/total*100,correct,total);
		if(flag_predict_probability)
			free(prob_estimates);
	}
}

void exit_with_help()
{
	printf(
	"Usage: predict [options] test_file model_file output_file\n"
	"options:\n"
	"-b probability_estimates: whether to output probability estimates, 0 or 1 (default 0); currently for logistic regression only\n"
	"-q : quiet mode (no outputs)\n"
	);
	exit(1);
}

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);

	int pid, total_processes;
	MPI_Comm_rank(MPI_COMM_WORLD, &pid);
	MPI_Comm_size(MPI_COMM_WORLD, &total_processes);

	FILE *input, *output;
	int i;
	char model_file[1024];

	// parse options
	for(i=1;i<argc;i++)
	{
		if(argv[i][0] != '-') break;
		++i;
		switch(argv[i-1][1])
		{
			case 'b':
				flag_predict_probability = atoi(argv[i]);
				break;
			case 'q':
				info = &print_null;
				i--;
				break;
			default:
				fprintf(stderr,"unknown option: -%c\n", argv[i-1][1]);
				exit_with_help();
				break;
		}
	}
	if(i>=argc)
		exit_with_help();

	input = fopen(argv[i],"r");
	if(input == NULL)
	{
		fprintf(stderr,"can't open input file %s\n",argv[i]);
		exit(1);
	}

	output = fopen(argv[i+2],"w");
	if(output == NULL)
	{
		fprintf(stderr,"can't open output file %s\n",argv[i+2]);
		exit(1);
	}

	for (int left = 0; left < BLOCK; left++) {
		sprintf(model_file,"L%d%s%d%s",left + 1, "_R", pid + 1,".model");
		printf("%s\n", model_file);
		if ((model_[left] = load_model(model_file)) == 0) {
			fprintf(stderr,"can't open model file %s\n",argv[i+1]);
			exit(1);
		}
	// }
	}

	x = (struct feature_node *) malloc(max_nr_attr*sizeof(struct feature_node));
	double start_time, end_time;
	start_time = MPI_Wtime();
	do_predict(input, output, pid);
	end_time = MPI_Wtime();
	printf("parallel exec time is %lf\n", end_time - start_time);
	for ( int left = 0; left < BLOCK; left++) {
		free_and_destroy_model(&model_[left]);
	}
	free(line);
	free(x);
	fclose(input);
	fclose(output);
	MPI_Finalize();
	return 0;
}

