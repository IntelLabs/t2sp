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
#include"Roofline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int DSPs() {
    FILE* fp;
    int _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen("acl_quartus_report.txt", "r")) == NULL) {
        printf("cannot open acl_quartus_report.txt!\n");
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
    return _ret;
}

double FMax() {
    FILE* fp;
    double _ret = 0;

    char str[STR_SIZE];
    if ((fp = fopen("acl_quartus_report.txt", "r")) == NULL) {
        printf("cannot open acl_quartus_report.txt!\n");
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
    return _ret;
}

double ExecTime() {
    FILE* fp;
    double _ret = 0;
  
    if ((fp = fopen("profile_info.txt", "r")) == NULL) {
        printf("cannot open file!\n");
        return _ret;
    }

    char str[STR_SIZE];
    while (fgets(str, 100, fp)) {
        double temp;
        sscanf(str, "%lf", &temp);
        if (temp > _ret)
            _ret = temp;
    }
    fclose(fp);
    return _ret;
}

void roofline(double mem_bandwidth, double compute_roof, double number_ops, double number_bytes, double exec_time) {
    char command[1000];
    if (exec_time == 0 || compute_roof == 0) {
        printf("Parameters Error!\n");
        return;
    }
    sprintf(command, "python $T2S_PATH/t2s/src/Roofline.py %lf %lf %lf %lf %lf", mem_bandwidth, compute_roof, number_ops, number_bytes, exec_time);
    printf("%s\n", command);
    int ret = system(command);
    assert(ret != -1);
}
