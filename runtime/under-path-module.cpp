#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>

#include "builtins.h"
#include "file.h"
#include "frame.h"
#include "handles.h"
#include "module-builtins.h"
#include "modules.h"
#include "os.h"
#include "symbols.h"

namespace py {

RawObject FUNC(_path, isdir)(Thread*, Arguments args) {
  CHECK(args.get(0).isStr(), "path must be str");
  unique_c_ptr<char> path(Str::cast(args.get(0)).toCStr());
  bool result = OS::dirExists(path.get());
  return Bool::fromBool(result);
}

RawObject FUNC(_path, isfile)(Thread*, Arguments args) {
  CHECK(args.get(0).isStr(), "path must be str");
  unique_c_ptr<char> path(Str::cast(args.get(0)).toCStr());
  bool result = OS::fileExists(path.get());
  return Bool::fromBool(result);
}

}  // namespace py
