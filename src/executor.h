#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int execute_pipeline(Pipeline *p);   /* returns exit status of last stage, -1 on error */

#endif