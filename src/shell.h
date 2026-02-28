/*
 * shell.h — Interactive command shell interface
 *
 * The shell is the user-facing layer of ExigeOS. It implements a
 * read-eval loop: print a prompt, read a line from the keyboard,
 * parse the command name and optional argument, dispatch to the
 * appropriate handler, and repeat forever.
 *
 * There is no process model, no file system, and no memory
 * allocation — every command runs directly in kernel context.
 */

#ifndef SHELL_H
#define SHELL_H

/* shell_run() — Enter the interactive shell loop.
 * Never returns: the loop runs until the machine is rebooted. */
void shell_run(void);

#endif
