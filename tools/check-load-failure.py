from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
c=Path((r/'.local/headless-cache').read_text().strip())
s=(c/'native-local/headless/core.cpp').read_text()
backing='    std::optional<Core::DeviceMemory> device_memory;'
kernel='    Kernel::KernelCore kernel;'
assert s.count(backing)==1 and s.index(backing)<s.index(kernel)
# Check the precise catch boundary from the frontend: original error is written
# while the owning core is alive, before stack unwinding begins.
m=(r/'headless/main.cpp').read_text()
start=m.index('                Core::SystemResultStatus loaded;')
end=m.index('                if (loaded !=',start)
body=m[start:end]
with tempfile.TemporaryDirectory(prefix='eden-load-failure-') as tmp:
 p=Path(tmp);src=p/'test.cpp';exe=p/'test'
 src.write_text('''#include <cstdio>
#include <stdexcept>
#include <cassert>
#include <string>
#include "network/shares.h"
bool core_failure=false, network_failure=false;
namespace Eden::Network {
std::string GameError() { return network_failure ? "injected network read failure" : ""; }
}
namespace Core { enum class SystemResultStatus { Success }; }
struct System { Core::SystemResultStatus Load(int,int,int) {
 if (core_failure) throw std::runtime_error("injected load failure");
 return Core::SystemResultStatus::Success;
} };
int main(int argc, char** argv) {
 assert(argc==2);
 core_failure=std::string(argv[1])=="core";
 network_failure=std::string(argv[1])=="network";
 System system; int window=0,guest=0,params=0; try {
'''+body+'''
 return core_failure || network_failure ? 2 : 0;
 } catch(const std::runtime_error& e) {
  const std::string expected=core_failure ? "injected load failure" :
      network_failure ? "injected network read failure" : "";
  return !expected.empty() && e.what()==expected ? 0 : 3;
 }
}
''')
 subprocess.run(['g++','-std=c++20','-I',str(r/'headless'),str(src),'-o',str(exe)],check=True)
 for mode,error in [('core','injected load failure'),('network','injected network read failure'),('success','')]:
  run=subprocess.run([str(exe),mode],capture_output=True,text=True)
  expected=f'Game load failed: {error}\n' if error else ''
  assert run.returncode==0 and run.stderr==expected, (mode,run.returncode,run.stderr)
print('Backing outlives kernel; core/network Load failures reported and rethrown, success retained PASS')

# Exercise the generated Load failure branch with a process owner whose
# destructor requires a live kernel, matching Service::OS::Process::Finalize.
a=s.index('        if (init_result != SystemResultStatus::Success) {')
b=s.index('        // Waiting for GPU', a)
branch=s[a:b]
assert 'process.reset();' in branch and branch.index('process.reset();') < branch.index('ShutdownMainProcess();')
assert s.count('            process.reset();\n            ShutdownMainProcess();') == 2
with tempfile.TemporaryDirectory(prefix='eden-process-order-') as tmp:
 p=Path(tmp);src=p/'test.cpp';exe=p/'test'
 src.write_text('''#include <memory>
#include <cassert>
#define LOG_CRITICAL(...) ((void)0)
enum class SystemResultStatus { Success, Error };
bool live=true;
struct Process { ~Process() { assert(live); } };
void ShutdownMainProcess() { live=false; }
SystemResultStatus load() {
 auto process=std::make_unique<Process>();
 auto init_result=SystemResultStatus::Error;
'''+branch+'''
 return SystemResultStatus::Success;
}
int main() { assert(load()==SystemResultStatus::Error); assert(!live); }
''')
 subprocess.run(['g++','-std=c++20',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Failed-load process released before kernel shutdown PASS')

# The game's content provider holds files of the system's file system. It is declared before the
# system (which keeps its address) and emptied before the system is destroyed; a provider that
# outlived a session released those files after their file system and crashed the next launch.
provider=m.index('            FileSys::ManualContentProvider game_contents;\n')
system=m.index('            Core::System system;\n')
clear=m.index('            SCOPE_EXIT { game_contents.ClearAllEntries(); };\n')
assert provider<system<clear and 'static FileSys::ManualContentProvider' not in m
with tempfile.TemporaryDirectory(prefix='eden-contents-order-') as tmp:
 p=Path(tmp);src=p/'test.cpp';exe=p/'test'
 src.write_text('''#include <cassert>
#include <memory>
#include <vector>
bool filesystem_alive=false;
struct File { ~File() { assert(filesystem_alive); } };
namespace FileSys { struct ManualContentProvider {
 std::vector<std::unique_ptr<File>> files;
 void ClearAllEntries() { files.clear(); }
}; }
namespace Core { struct System { System() { filesystem_alive=true; } ~System() { filesystem_alive=false; } }; }
template <class F> struct Exit { F f; ~Exit() { f(); } };
struct MakeExit { template <class F> Exit<F> operator+(F f) { return {f}; } };
#define SCOPE_EXIT auto scope_exit = MakeExit{} + [&]()
int main() { for (int session=0; session<2; ++session) {
'''+m[provider:clear]+'''            SCOPE_EXIT { game_contents.ClearAllEntries(); };
            game_contents.files.push_back(std::make_unique<File>());
 } }
''')
 subprocess.run(['g++','-std=c++20',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Game contents released before the core and its file system PASS')
