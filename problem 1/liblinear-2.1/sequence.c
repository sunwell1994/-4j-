#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <string.h>
#include <ctype.h>
#include <errno.h>

static int (*info)(const char *fmt,...) = &printf;


static char *line = NULL;
static int max_line_len;

int main(int argc, char **argv)
{

	FILE *pred = fopen("out.txt", "r");

	FILE *tar = fopen("label.txt", "r");
	FILE *new = fopen("roc.txt","w");
	fprintf(new, "{");



	int *predict ;
	int target ;
	double p; 
	for ( double i = -0.8; i <= 1.8; i = i + 0.02) {
		// printf("right here %f\n", i );
		int TP = 0;
		int TN = 0;
		int FP = 0;
		int FN = 0;
		int correct = 0;
		int total =0;

		while(fscanf(pred, "%lf\n", &p) != -1 && fscanf(tar, "%d\n", &target) != -1) {
		// char *p = strtok(line," \t"); // label, 分割字符串来分别统计
		// p = strtok(NULL," \t");
					// printf("right %lf\n", i );

			if (p <= i) {
				if(target == 0) {
					TP++;
				}
				else {
					FP++;
				}
			}
			else {
				if (target == 1) {
					TN++;
				}
				else {
					FN++;
				}
			}

		}
		correct = TP +TN;
		total = correct + FP + FN;

		printf("************************************%lf************************************\n", i);

		info("Accuracy = %g%% (%d/%d)  ", (double) correct / total * 100, correct, total);
		// info("TPR = %g\n", (double) TP / (TP + FN));
		// info("FPR = %g\n", (double) FP / (FP + TN));
		fprintf(new, "{%lf, %lf},", (double) FP / (FP + TN), (double) TP / (TP + FN) );
		double p = (double) TP/(TP+FP);
		double r = (double) TP/(TP+FN);
		double lzb_F1 = 2.0*r*p/(r+p);
		info("F1 = %g  ",lzb_F1);
		info("TP = %d  ",TP);
		info("FP = %d  ",FP);
		info("TN = %d  ",TN);
		info("FN = %d\n",FN);


		rewind(pred);
		rewind(tar);
	}
	
		fprintf(new, "}");

	// new->close();
		
	
	return 0;
}

