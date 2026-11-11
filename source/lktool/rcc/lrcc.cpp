#include "lrccwriter.h"
#include "lkcore/io/liofileman.h"
#include "lkcore/ldebug.h"
#include "lrccwriter.h"
#include "lkcore/3rd/cmdparser.hpp"

#if LOOKI_NAMESPACE
using namespace lk;
#endif

void configureParser(cli::Parser& parser) {
    parser.set_required<std::string>("i", "input", "Input lrc file.");
    parser.set_optional<std::string>("o", "output", "", "Output cpp file or directory.");
}


int main(int argc, char* argv[]) {

    cli::Parser parser(argc, argv);
    configureParser(parser);
    if(parser.run() == false){
        return 0;
    }

    LRccWriter writer;
    writer.m_compress = true;
    writer.m_format = LRccWriter::CppCode;

    std::string input = parser.get<std::string>("i");
    if(input.empty()){
        return 0;
    }

    std::filesystem::path inPath(input);

    if(inPath.is_relative()){
        inPath = std::filesystem::absolute(inPath);
    }


    std::string output = parser.get<std::string>("o");
    std::filesystem::path outPath;
    if(output.empty()){
        outPath = inPath.parent_path() / (inPath.stem().string() + ".cpp");
    }else{
        outPath = output;
        if(std::filesystem::is_directory(outPath)){
            outPath = outPath / (inPath.stem().string() + ".cpp");
        }else{
            if(outPath.string().ends_with(".cpp") == false){
                outPath +=".cpp";
            }
        }
    }
    if(outPath.is_relative()){
        outPath = std::filesystem::absolute(outPath);
    }

    std::filesystem::path pPath= outPath.parent_path();
    if(std::filesystem::exists(pPath) == false){
        if(std::filesystem::create_directory(pPath) == false){
            lWarn() << "Create path failed:" << pPath.string();
            return -1;
        }
    }

    writer.m_rccFiles.push_back(inPath.string());
    writer.readFiles();
    writer.output(outPath.string());
	return 0;
}
