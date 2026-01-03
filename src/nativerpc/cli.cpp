/**
 *  Native RPC Cli
 * 
 *      main
 */
#include "common.h"

using namespace nativerpc;

int main(int argc, char* argv[])
{
    std::vector<std::string> arguments(argv, argv + argc);
    std::filesystem::path p = __FILE__;
    assert(std::filesystem::exists((p.parent_path().parent_path().parent_path() / "src/nativerpc/cli.js").string()));
    std::string cmd = std::string() + "node src/nativerpc/cli.js " + joinString(arguments, " ", 1);
    std::string cwd = p.parent_path().parent_path().parent_path().string();
    auto resp = execProcess(cmd, cwd);
    std::cout << resp << std::endl << std::flush;
    return 0;
}

