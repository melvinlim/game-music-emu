#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <term.h>

#include <iostream>
#include <filesystem>
#include <string>
#include <list>

//	https://stackoverflow.com/questions/2347770/how-do-you-clear-the-console-screen-in-c
void ClearScreen();

std::list<std::string> getFileList();

#endif
