/*
README

Compile it on Cygwin: 

PSTREAM_ROOT=/home/philippe/pstreams-1.0.1
g++ -Wall -Wextra -Wpedantic -pthread --std=gnu++14 try_pstream.cpp -I$PSTREAM_ROOT


*/

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <functional>
#include <vector>
#include <set>
#include <fstream>
#include <sstream>
#include <regex>

#include <pstream.h>

void example()
{
	// print names of all header files in current directory
	redi::ipstream in("node to_markdown.js");
	std::string str;
	while (in >> str) {
	    std::cout << str << std::endl;
	}
}

void launch_to_markdown()
{
	using namespace std;

	ifstream test_file_in("test.html");
	string content;
	string line;
	while (getline(test_file_in, line))
	{
	    content = content + line + "\n";
	}

	//cout << content;

	using namespace redi;

	vector<string> args;
	args.push_back("node");
	args.push_back("to_markdown.js");

	pstream in("node", args, pstreambuf::pstdin | pstreambuf::pstdout);
	in << content;
	peof(in); // close stdin so client knows to finish

	string markdown;
	while( getline(in, line) )
	{
		markdown = markdown + line + "\n";
	}	

	cout << markdown;
}

int main(int, char **)
{
	// Load the system locale.
	setlocale(LC_ALL, "");
	launch_to_markdown();
	//example();
	return 0;
}
