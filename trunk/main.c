/* 
 * File:   main.c
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:06
 */

#include "trietool.h"

/*
 * 
 */
int main(int argc, char **argv) {

    ProgEnv env;
    init_program_env(argc, argv, &env);
    init_sock_accept_env(&env);
    program_exit(&env);

    return (EXIT_SUCCESS);
}

