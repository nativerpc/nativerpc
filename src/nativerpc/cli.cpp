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
    setEnvVar("FORCE_COLOR", "true");
    std::string cmd = std::string() + "node " + p.parent_path().parent_path().parent_path().string() + "/src/nativerpc/cli.js " + joinString(arguments, " ", 1);
    execShell(cmd);
    return 0;
}

