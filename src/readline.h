/*
 * readline.h - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef READLINE_H
#define READLINE_H

#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>

/* TODO: implement completion */
void rl_completor(void);

#endif
