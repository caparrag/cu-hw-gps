#include <iostream>
#include <boost/regex.hpp>
#include <boost/program_options.hpp>
#include <vector>
#include <map>
#include <ostream>
#include <fstream>
#include "config.h"
#include "macro_entry.h"
#include "expression.h"
#include "input_parser.h"

namespace opt=boost::program_options;
using namespace std;

int main(int argc, char *argv[])
{
    opt::options_description visibleOptions("Allowed options");
    visibleOptions.add_options()
        ("help,h","Display this help message.")
        ("output,o",opt::value<string>(),"Write output to specified file.")
        ("undef,u","Generate an undefines file.")
        ("version,v","Show version information.");
    opt::options_description options("All program options");
    options.add(visibleOptions);
    options.add_options()("input",opt::value<vector<string> >(),"Input...");
    opt::positional_options_description pos;
    pos.add("input",-1);
    
    opt::variables_map vm;
    opt::store(opt::command_line_parser(argc,argv).options(options).positional(pos).run(),vm);
    opt::notify(vm);

    if(vm.count("help"))
    {
        cout<<"Usage: "<<argv[0]<<" [OPTION]... [FILE]..."<<endl
            <<"Generate Verilog include file from XML variable"<<endl
            <<"definitions file."<<endl
            <<endl<<visibleOptions<<endl
            <<"If supplied, input is read from the listed files."<<endl
            <<"By default, input is taken from stdin and output"<<endl
            <<"is written to stdout."<<endl;
        return 0;
    }
    else if(vm.count("version"))
    {
        cout<<PACKAGE_STRING<<endl
            <<"Compiled "<<__DATE__<<" "<<__TIME__<<"."<<endl
            <<endl
            <<"Report bugs to "<<PACKAGE_BUGREPORT<<"."<<endl;
        return 0;
    }

    //Parse input files.
    map<string,MacroEntry*> vars;
    vector<string> verilog;
    int errorCount=0;
    if(vm.count("input"))
    {
        vector<string> inputFiles=vm["input"].as<vector<string> >();
        for(vector<string>::iterator i=inputFiles.begin();
            i!=inputFiles.end();
            i++)
        {
            InputParser in(*i);
            errorCount+=in.Parse(vars,verilog);
        }
    }
    else
    {
        InputParser in;
        errorCount+=in.Parse(vars,verilog);
    }

    //Print output file.
    if(errorCount==0)
    {
        ofstream outFile;
        ostream *out=NULL;
        if(!vm.count("output"))out=&cout;
        else
        {
            outFile.open(vm["output"].as<string>().c_str());
            if(outFile.good())out=&outFile;
            else cerr<<"Error: unable to open output file '"
                     <<vm["output"].as<string>()<<"'."<<endl;
        }
        if(out!=NULL)
        {
            int errorCount=0;

            string output;
            output="//Generated by "+string(PACKAGE_STRING)+", compiled on "+string(__DATE__)+" "+string(__TIME__)+".\n";
            output+="//This file has been automatically generated.\n";
            output+="//Edit contents with extreme caution.\n\n";

            if(!vm.count("undef") && verilog.size()>0)
            {
                for(vector<string>::iterator i=verilog.begin();
                    i!=verilog.end();
                    i++)
                {
                    output+=(*i)+"\n";
                }
                output+="\n";
            }

            map<string,Expression*> expList;
            for(map<string,MacroEntry*>::iterator i=vars.begin();
                i!=vars.end();
                i++)
            {
                expList[(*i).first]=(*i).second->expression;
            }

            boost::regex newline("\\n");
            for(map<string,MacroEntry*>::iterator i=vars.begin();
                i!=vars.end();
                i++)
            {
                string variable=(*i).first;
                MacroEntry *entry=(*i).second;

                if(vm.count("undef"))
                {
                    output+="`ifdef "+variable+"\n";
                    output+=" `undef "+variable+"\n";
                    output+="`endif\n";
                    output+="\n";
                }
                else
                {
                    if(!entry->print)continue;
                    try
                    {
                        if(entry->comments!="")
                        {
                            output+="//";
                            output+=boost::regex_replace(entry->comments,newline,"\\n//");
                            output+="\n";
                        }
                        output+="`ifndef "+variable+"\n";
                        output+=" `define "+variable+" "+entry->expression->Value(expList)+"\n";
                        output+="`endif\n";
                        output+="\n";
                    }
                    catch(Expression::ExpressionError &e)
                    {
                        errorCount++;
                        e.SetVariable(variable);
                        cerr<<e.what()<<endl;
                    }
                }
            }

            if(errorCount==1)cerr<<"Found 1 error."<<endl;
            else if(errorCount>0)cerr<<"Found "<<errorCount<<" errors."<<endl;
            else (*out)<<output;

            if(vm.count("output"))outFile.close();
        }
    }
    else
    {
        if(errorCount==1)cerr<<"Found 1 error."<<endl;
        else if(errorCount>0)cerr<<"Found "<<errorCount<<" errors."<<endl;
    }

    //Cleanup expressions.
    for(map<string,MacroEntry*>::iterator i=vars.begin();
        i!=vars.end();
        i++)
    {
        delete (*i).second;
    }
    
    return errorCount!=0;
}
