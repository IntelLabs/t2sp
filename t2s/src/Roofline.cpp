/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Roofline.h"
#include "SharedUtilsInC.h"

int DSPs() {
    char *quartus_output_dir = quartus_output_directory();
    char *report_file = concat_directory_and_file(quartus_output_dir, "acl_quartus_report.txt");

    FILE* fp;
    int _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen(report_file, "r")) == NULL) {
        printf("cannot open quartus report: %s! \n", report_file);
        free(quartus_output_dir);
        free(report_file);
        return -1;
    }
    while (fgets(str, 100, fp)) {
        char* pos = strchr(str, ':');
        if (pos == NULL)
            continue;

        char str1[STR_SIZE], str2[STR_SIZE];
        assert(pos - str < STR_SIZE);
        assert(strlen(pos + 2) < STR_SIZE);
        strncpy(str1, str, pos - str); str1[pos - str] = '\0';
        strncpy(str2, pos + 2, strlen(pos + 2)); str2[strlen(pos + 2)] = '\0';

        if (strcmp(str1, "DSP blocks") == 0) {
            char* p = strtok(str2, "/");
            char number[STR_SIZE];
            number[0] = '\0';

            char* q = strtok(p, ",");
            while (q != NULL) {
                strncat(number, q, strlen(q));
                q = strtok(NULL, ",");
            }

            number[strlen(number) - 1] = '\0';
            sscanf(number, "%d", &_ret);
            break;
        }

    }
    fclose(fp);
    free(quartus_output_dir);
    free(report_file);
    return _ret;
}

double FMax() {
    char *quartus_output_dir = quartus_output_directory();
    char *report_file = concat_directory_and_file(quartus_output_dir, "acl_quartus_report.txt");

    FILE* fp;
    double _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen(report_file, "r")) == NULL) {
        printf("cannot open quartus report: %s! \n", report_file);
        free(quartus_output_dir);
        free(report_file);
        return -1;
    }
    while (fgets(str, 100, fp)) {
        char* pos = strchr(str, ':');
        if (pos == NULL)
            continue;

        char str1[STR_SIZE], str2[STR_SIZE];
        assert(pos - str < STR_SIZE);
        assert(strlen(pos + 2) < STR_SIZE);
        strncpy(str1, str, pos - str); str1[pos - str] = '\0';
        strncpy(str2, pos + 2, strlen(pos + 2)); str2[strlen(pos + 2)] = '\0';

        if (strcmp(str1, "Kernel fmax") == 0) {
            sscanf(str2, "%lf", &_ret);
            break;
        }

    }
    fclose(fp);
    free(quartus_output_dir);
    free(report_file);
    return _ret;
}

// Execution time in terms of nanoseconds
double ExecTime(const char* kernel_name) {
    char *bitstream_dir = bitstream_directory();
    char *exec_time_file = concat_directory_and_file(bitstream_dir, "exec_time.txt");

    FILE* fp;
    double _ret = 0;
  
    if ((fp = fopen(exec_time_file, "r")) == NULL) {
        printf("Cannot open %s!\n", exec_time_file);
    } else {
        fscanf(fp, "%lf", &_ret);
        if (kernel_name) {
            char tmp_s[100];
            double tmp_t;
            while (fscanf(fp, "%s %lf\n", tmp_s, &tmp_t) != EOF) {
                if (strcmp(tmp_s, kernel_name) == 0) {
                    printf("kernel %s exec time: %lf\n", tmp_s, tmp_t);
                    _ret = tmp_t;
                    break;
                }
            }
        }
    }
    fclose(fp);
    free(bitstream_dir);
    free(exec_time_file);
    return _ret;
}

void roofline(double mem_bandwidth, double compute_roof, double number_ops, double number_bytes, double exec_time) {
    char command[1000];
    if (exec_time == 0 || compute_roof == 0) {
        printf("Roofline parameters Error!\n");
        return;
    }
    printf("GFlops: %lf\n", number_ops / exec_time);
    sprintf(command, "python $T2S_PATH/t2s/src/Roofline.py %lf %lf %lf %lf %lf", mem_bandwidth, compute_roof, number_ops, number_bytes, exec_time);
    printf("Run command: %s\n", command);
    int ret = system(command);
    assert(ret != -1);
}

int DSPs_oneapi() {
    char *quartus_output_dir = quartus_output_directory_oneapi();
    char *report_file = concat_simple(quartus_output_dir, ".prj/acl_quartus_report.txt");

    FILE* fp;
    int _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen(report_file, "r")) == NULL) {
        printf("cannot open quartus report: %s! \n", report_file);
        free(quartus_output_dir);
        free(report_file);
        return -1;
    }
    while (fgets(str, 100, fp)) {
        char* pos = strchr(str, ':');
        if (pos == NULL)
            continue;

        char str1[STR_SIZE], str2[STR_SIZE];
        assert(pos - str < STR_SIZE);
        assert(strlen(pos + 2) < STR_SIZE);
        strncpy(str1, str, pos - str); str1[pos - str] = '\0';
        strncpy(str2, pos + 2, strlen(pos + 2)); str2[strlen(pos + 2)] = '\0';

        if (strcmp(str1, "DSP blocks") == 0) {
            char* p = strtok(str2, "/");
            char number[STR_SIZE];
            number[0] = '\0';

            char* q = strtok(p, ",");
            while (q != NULL) {
                strncat(number, q, strlen(q));
                q = strtok(NULL, ",");
            }

            number[strlen(number) - 1] = '\0';
            sscanf(number, "%d", &_ret);
            break;
        }

    }
    fclose(fp);
    free(quartus_output_dir);
    free(report_file);
    return _ret;
}

double FMax_oneapi() {
    char *quartus_output_dir = quartus_output_directory_oneapi();
    char *report_file = concat_simple(quartus_output_dir, ".prj/acl_quartus_report.txt");

    FILE* fp;
    double _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen(report_file, "r")) == NULL) {
        printf("cannot open quartus report: %s! \n", report_file);
        free(quartus_output_dir);
        free(report_file);
        return -1;
    }
    while (fgets(str, 100, fp)) {
        char* pos = strchr(str, ':');
        if (pos == NULL)
            continue;

        char str1[STR_SIZE], str2[STR_SIZE];
        assert(pos - str < STR_SIZE);
        assert(strlen(pos + 2) < STR_SIZE);
        strncpy(str1, str, pos - str); str1[pos - str] = '\0';
        strncpy(str2, pos + 2, strlen(pos + 2)); str2[strlen(pos + 2)] = '\0';

        if (strcmp(str1, "Kernel fmax") == 0) {
            sscanf(str2, "%lf", &_ret);
            break;
        }

    }
    fclose(fp);
    free(quartus_output_dir);
    free(report_file);
    return _ret;
}
