#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "linear.h"
#include "mpi.h"
#define Malloc(type,n) (type *)malloc((n)*sizeof(type))
#define INF HUGE_VAL

void print_null(const char *s) {}

void exit_with_help()
{
	printf(
	"Usage: train [options] training_set_file [model_file]\n"
	"options:\n"
	"-s type : set type of solver (default 1)\n"
	"  for multi-class classification\n"
	"	 0 -- L2-regularized logistic regression (primal)\n"
	"	 1 -- L2-regularized L2-loss support vector classification (dual)\n"
	"	 2 -- L2-regularized L2-loss support vector classification (primal)\n"
	"	 3 -- L2-regularized L1-loss support vector classification (dual)\n"
	"	 4 -- support vector classification by Crammer and Singer\n"
	"	 5 -- L1-regularized L2-loss support vector classification\n"
	"	 6 -- L1-regularized logistic regression\n"
	"	 7 -- L2-regularized logistic regression (dual)\n"
	"  for regression\n"
	"	11 -- L2-regularized L2-loss support vector regression (primal)\n"
	"	12 -- L2-regularized L2-loss support vector regression (dual)\n"
	"	13 -- L2-regularized L1-loss support vector regression (dual)\n"
	"-c cost : set the parameter C (default 1)\n"
	"-p epsilon : set the epsilon in loss function of SVR (default 0.1)\n"
	"-e epsilon : set tolerance of termination criterion\n"
	"	-s 0 and 2\n"
	"		|f'(w)|_2 <= eps*min(pos,neg)/l*|f'(w0)|_2,\n"
	"		where f is the primal function and pos/neg are # of\n"
	"		positive/negative data (default 0.01)\n"
	"	-s 11\n"
	"		|f'(w)|_2 <= eps*|f'(w0)|_2 (default 0.001)\n"
	"	-s 1, 3, 4, and 7\n"
	"		Dual maximal violation <= eps; similar to libsvm (default 0.1)\n"
	"	-s 5 and 6\n"
	"		|f'(w)|_1 <= eps*min(pos,neg)/l*|f'(w0)|_1,\n"
	"		where f is the primal function (default 0.01)\n"
	"	-s 12 and 13\n"
	"		|f'(alpha)|_1 <= eps |f'(alpha0)|,\n"
	"		where f is the dual function (default 0.1)\n"
	"-B bias : if bias >= 0, instance x becomes [x; bias]; if < 0, no bias term added (default -1)\n"
	"-wi weight: weights adjust the parameter C of different classes (see README for details)\n"
	"-v n: n-fold cross validation mode\n"
	"-C : find parameter C (only for -s 0 and 2)\n"
	"-q : quiet mode (no outputs)\n"
	);
	exit(1);
}

void exit_input_error(int line_num)
{
	fprintf(stderr,"Wrong input format at line %d\n", line_num);
	exit(1);
}

static char *line = NULL;
static int max_line_len;
static int start[] = {0, 6622, 7066, 7145, 11812, 11898, 13205, 13357, 14050, 14476, 15575, 15809, 23785, 45114, 45591, 54334, 61344, 62668, 63294, 63494, 67845, 68212, 68888, 70215, 71363, 75731, 78177, 86412, 89150, 90950, 92185, 92966, 93855, 106080, 106530, 107067, 109924, 128683, 131082, 131909, 132249, 148360, 149470, 155152, 156189, 156730, 176919, 180403, 180976, 181020, 181345, 181387, 183056, 185686, 187356, 191187, 191271, 191325, 198360, 220984, 229921, 233923, 234930, 239236, 239246, 239249, 240878, 244199, 248653, 250440, 251148, 251873, 252421, 252994, 253671, 254453, 256719, 256770, 257331};

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

void parse_command_line(int argc, char **argv, char *input_file_name, char *model_file_name);
void read_problem(const char *filename, int left, int right);
void do_cross_validation();
void do_find_parameter_C();
void new_train (char *model_file, char *input_file_name, char *model_file_name, int pid, int right) ;

struct feature_node *x_space;
struct parameter param;
struct problem prob;
struct model* model_;
int flag_cross_validation;
int flag_find_C;
int flag_C_specified;
int flag_solver_specified;
int nr_fold;
double bias;

int main(int argc, char **argv)
{
	char input_file_name[1024];
	char model_file_name[1024];
	char model_file[1024];
	const char *error_msg;
	int right;
	parse_command_line(argc, argv, input_file_name, model_file_name);

	int pid, total_processes;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &pid);
	MPI_Comm_size(MPI_COMM_WORLD, &total_processes);
	for ( right = 15; right < 78; right++) {
		new_train(model_file, input_file_name, model_file_name, pid, right);
	}

	MPI_Finalize();

	return 0;
}

void new_train (char *model_file, char *input_file_name, char *model_file_name, int pid, int right) {
	sprintf(model_file,"%d%s%d%s",pid + 1, "_", right + 1,".model");

	read_problem(input_file_name, pid, right);

	if (flag_find_C)
	{
		do_find_parameter_C();
	}
	else if(flag_cross_validation)
	{
		do_cross_validation();
	}
	else
	{
		printf("train pid: %d \n", pid);

		model_=train(&prob, &param);
		printf("finish train pid: %d \n", pid);

		if(save_model(model_file, model_))
		{
			fprintf(stderr,"can't save model to file %s\n",model_file);
			exit(1);
		}
		free_and_destroy_model(&model_);
	}
	destroy_param(&param);
	free(prob.y);
	free(prob.x);
	free(x_space);
	free(line);
}

void do_find_parameter_C()
{
	double start_C, best_C, best_rate;
	double max_C = 1024;
	if (flag_C_specified)
		start_C = param.C;
	else
		start_C = -1.0;
	printf("Doing parameter search with %d-fold cross validation.\n", nr_fold);
	find_parameter_C(&prob, &param, nr_fold, start_C, max_C, &best_C, &best_rate);
	printf("Best C = %g  CV accuracy = %g%%\n", best_C, 100.0*best_rate);
}

void do_cross_validation()
{
	int i;
	int total_correct = 0;
	double total_error = 0;
	double sumv = 0, sumy = 0, sumvv = 0, sumyy = 0, sumvy = 0;
	double *target = Malloc(double, prob.l);

	cross_validation(&prob,&param,nr_fold,target);
	if(param.solver_type == L2R_L2LOSS_SVR ||
	   param.solver_type == L2R_L1LOSS_SVR_DUAL ||
	   param.solver_type == L2R_L2LOSS_SVR_DUAL)
	{
		for(i=0;i<prob.l;i++)
		{
			double y = prob.y[i];
			double v = target[i];
			total_error += (v-y)*(v-y);
			sumv += v;
			sumy += y;
			sumvv += v*v;
			sumyy += y*y;
			sumvy += v*y;
		}
		printf("Cross Validation Mean squared error = %g\n",total_error/prob.l);
		printf("Cross Validation Squared correlation coefficient = %g\n",
				((prob.l*sumvy-sumv*sumy)*(prob.l*sumvy-sumv*sumy))/
				((prob.l*sumvv-sumv*sumv)*(prob.l*sumyy-sumy*sumy))
			  );
	}
	else
	{
		for(i=0;i<prob.l;i++)
			if(target[i] == prob.y[i])
				++total_correct;
		printf("Cross Validation Accuracy = %g%%\n",100.0*total_correct/prob.l);
	}

	free(target);
}

void parse_command_line(int argc, char **argv, char *input_file_name, char *model_file_name)
{
	int i;
	void (*print_func)(const char*) = NULL;	// default printing to stdout

	// default values
	param.solver_type = L2R_L2LOSS_SVC_DUAL;
	param.C = 1;
	param.eps = INF; // see setting below
	param.p = 0.1;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;
	param.init_sol = NULL;
	flag_cross_validation = 0;
	flag_C_specified = 0;
	flag_solver_specified = 0;
	flag_find_C = 0;
	bias = -1;

	// parse options
	for(i=1;i<argc;i++)
	{
		if(argv[i][0] != '-') break;
		if(++i>=argc)
			exit_with_help();
		switch(argv[i-1][1])
		{
			case 's':
				param.solver_type = atoi(argv[i]);
				flag_solver_specified = 1;
				break;

			case 'c':
				param.C = atof(argv[i]);
				flag_C_specified = 1;
				break;

			case 'p':
				param.p = atof(argv[i]);
				break;

			case 'e':
				param.eps = atof(argv[i]);
				break;

			case 'B':
				bias = atof(argv[i]);
				break;

			case 'w':
				++param.nr_weight;
				param.weight_label = (int *) realloc(param.weight_label,sizeof(int)*param.nr_weight);
				param.weight = (double *) realloc(param.weight,sizeof(double)*param.nr_weight);
				param.weight_label[param.nr_weight-1] = atoi(&argv[i-1][2]);
				param.weight[param.nr_weight-1] = atof(argv[i]);
				break;

			case 'v':
				flag_cross_validation = 1;
				nr_fold = atoi(argv[i]);
				if(nr_fold < 2)
				{
					fprintf(stderr,"n-fold cross validation: n must >= 2\n");
					exit_with_help();
				}
				break;

			case 'q':
				print_func = &print_null;
				i--;
				break;

			case 'C':
				flag_find_C = 1;
				i--;
				break;

			default:
				fprintf(stderr,"unknown option: -%c\n", argv[i-1][1]);
				exit_with_help();
				break;
		}
	}

	set_print_string_function(print_func);

	// determine filenames
	if(i>=argc)
		exit_with_help();

	strcpy(input_file_name, argv[i]);

	if(i<argc-1)
		strcpy(model_file_name,argv[i+1]);
	else
	{
		char *p = strrchr(argv[i],'/');
		if(p==NULL)
			p = argv[i];
		else
			++p;
		sprintf(model_file_name,"%s.model",p);
	}

	// default solver for parameter selection is L2R_L2LOSS_SVC
	if(flag_find_C)
	{
		if(!flag_cross_validation)
			nr_fold = 5;
		if(!flag_solver_specified)
		{
			fprintf(stderr, "Solver not specified. Using -s 2\n");
			param.solver_type = L2R_L2LOSS_SVC;
		}
		else if(param.solver_type != L2R_LR && param.solver_type != L2R_L2LOSS_SVC)
		{
			fprintf(stderr, "Warm-start parameter search only available for -s 0 and -s 2\n");
			exit_with_help();
		}
	}

	if(param.eps == INF)
	{
		switch(param.solver_type)
		{
			case L2R_LR:
			case L2R_L2LOSS_SVC:
				param.eps = 0.01;
				break;
			case L2R_L2LOSS_SVR:
				param.eps = 0.001;
				break;
			case L2R_L2LOSS_SVC_DUAL:
			case L2R_L1LOSS_SVC_DUAL:
			case MCSVM_CS:
			case L2R_LR_DUAL:
				param.eps = 0.1;
				break;
			case L1R_L2LOSS_SVC:
			case L1R_LR:
				param.eps = 0.01;
				break;
			case L2R_L1LOSS_SVR_DUAL:
			case L2R_L2LOSS_SVR_DUAL:
				param.eps = 0.1;
				break;
		}
	}
}

// read in a problem (in libsvm format)
void read_problem(const char *filename, int left, int right)
{
	int max_index, inst_max_index, i;
	size_t elements, j;
	FILE *fp = fopen(filename,"r");
	char *endptr;
	char *idx, *val, *label;
	int pt, lStart, rStart, left_size, right_size;

	if(fp == NULL)
	{
		fprintf(stderr,"can't open input file %s\n",filename);
		exit(1);
	}

	max_line_len = 4096;
	
	lStart = start[left];
	rStart = start[right];
	left_size = start[left + 1] - lStart;
	right_size = start[right + 1] - rStart;
	// printf("%d-%d : ls%d rs%d LS%d RS%d \n", left, right, left_size, right_size, lStart, rStart);

	line = Malloc(char,max_line_len);

	prob.l = left_size + right_size;
	elements = 10000000;

	prob.bias=bias;

	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct feature_node *,prob.l);
	x_space = Malloc(struct feature_node,elements+prob.l);

	max_index = 0;
	j=0;
	pt = 0;
	// printf("%d-%d : %ld\n", left, right, r_fp - l_fp );
	for(i=0;i<prob.l;i++)
	{
		inst_max_index = 0; // strtol gives 0 if wrong format
		if(i == 0) {
			for (pt = 0; pt < lStart; pt++) {
				readline(fp);
			}
		}
			// fseek(fp, lStart, SEEK_SET);
		else if (i == left_size) {
			for (pt += left_size; pt < rStart; pt++) {
				readline(fp);
			}
		}
			
		readline(fp);
		// if (i == left_size) printf("%d-%d : %s\n", left, right, line );
		prob.x[i] = &x_space[j]; //多维特征指针
		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(i+1);

		// prob.y[i] = strtod(label,&endptr);
		// if(endptr == label || *endptr != '\0')
		// 	exit_input_error(i+1);
		// if (i > left_size - 5 && i < left_size + 5) printf("%d-%d : %c\n", left, right, label[0] );
		switch (label[0]) {
			case 'A': prob.y[i] = 0; break;
			case 'B': prob.y[i] = 1; break;
			case 'C': prob.y[i] = 1; break;
			case 'D': prob.y[i] = 1; break;
		}

		// if (i > prob.l - 30000) printf("%d : begin while%d\n", left * 1 + right, i );
		while(1)
		{
			idx = strtok(NULL,":");
			val = strtok(NULL," \t");

			if(val == NULL)
				break;

			errno = 0;
			x_space[j].index = (int) strtol(idx,&endptr,10);
			if(endptr == idx || errno != 0 || *endptr != '\0' || x_space[j].index <= inst_max_index)
				exit_input_error(i+1);
			else
				inst_max_index = x_space[j].index;

			errno = 0;
			x_space[j].value = strtod(val,&endptr);
			if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
				exit_input_error(i+1);

			++j;
		}
		// if (i > prob.l - 30000) printf("%d : end%d\n", left * 1 + right, i );


		if(inst_max_index > max_index)
			max_index = inst_max_index;

		if(prob.bias >= 0)
			x_space[j++].value = prob.bias;

		x_space[j++].index = -1;
		// if (i > prob.l - 30000) printf("%d : trueend%d\n", left * 1 + right, i );

	}

	if(prob.bias >= 0)
	{
		prob.n=max_index+1;
		for(i=1;i<prob.l;i++)
			(prob.x[i]-2)->index = prob.n;
		x_space[j-2].index = prob.n;
	}
	else
		prob.n=max_index;

	fclose(fp);
}
