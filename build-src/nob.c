#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#ifdef _WIN32
#define OS_SEP "\\"
#else
#define OS_SEP "/"
#endif

#define LOCAL_FOLDER(x) "." OS_SEP #x OS_SEP

typedef struct {
  const char* const* items;
  size_t count;
} const_str_list_t;

#define CSTR_LIST(...) { \
  .items = (const char*[]){ __VA_ARGS__ }, \
  .count = (sizeof((const char*[]){ __VA_ARGS__ }) / sizeof(const char*)) \
}
#define CSTR_LIST_EMPTY { .items = NULL, .count = 0 }

//////////////////////////////////////////////////

// DEFINE BUILD PARAMETERS HERE 

// Folders
#define SRC_FOLDER         LOCAL_FOLDER(src)
#define BUILD_FOLDER       LOCAL_FOLDER(build)
#define BIN_FOLDER         LOCAL_FOLDER(bin)
#define THIRD_PARTY_FOLDER LOCAL_FOLDER(third_party)

#define PROJ_NAME "main" 

// Supported Toolchains
typedef enum {
  T_MSVC,
  T_GNU,

  COUNT_TOOLCHAINS
} tc_t;

const char* tc_names[] = {
  [T_MSVC] = "MSVC",
  [T_GNU]  = "GNU",
};

const char* tc_cc[] = {
  [T_MSVC] = "cl",
  [T_GNU]  = "gcc",
};

// Flags per toolchain
// Edit these to change compilation settings and parameters
const_str_list_t tc_cflags[] = {
  [T_MSVC] = CSTR_LIST("/nologo", "/W4", "/diagnostics:caret", "/D_CRT_SECURE_NO_WARNINGS"),
  [T_GNU]  = CSTR_LIST("-Wall", "-Wextra", "-Wswitch-enum", "-ggdb"),
};

const_str_list_t tc_lflags[] = {
  [T_MSVC] = CSTR_LIST("/nologo"),
  [T_GNU]  = CSTR_LIST("-ggdb", "-rdynamic"),
};

const_str_list_t tc_includes[] = {
  [T_MSVC] = CSTR_LIST(THIRD_PARTY_FOLDER "libffi" OS_SEP, SRC_FOLDER),
  [T_GNU]  = CSTR_LIST(THIRD_PARTY_FOLDER "libffi" OS_SEP, SRC_FOLDER),
};

const_str_list_t tc_lib_path[] = {
  [T_MSVC] = CSTR_LIST(THIRD_PARTY_FOLDER "libffi" OS_SEP),
  [T_GNU]  = CSTR_LIST_EMPTY, 
};

const_str_list_t tc_libs[] = {
  [T_MSVC] = CSTR_LIST("libffi-8.lib"),
  [T_GNU]  = CSTR_LIST("ffi"),
};

// Toolchain specific flags/parameters
// Edit these if adding a new toolchain, otherwise these SHOULD NOT BE CUSTOMIZED
const char* tc_out_obj[] = {
  [T_MSVC] = "/Fo:",
  [T_GNU]  = "-o",
};

const char* tc_out_exe[] = {
  [T_MSVC] = "/Fe:",
  [T_GNU]  = "-o",
};

const char* tc_include_flag[] = {
  [T_MSVC] = "/I",
  [T_GNU]  = "-I",
};

const char* tc_compile_flag[] = {
  [T_MSVC] = "/c",
  [T_GNU]  = "-c",
};

const char* tc_lib_flag[] = {
  [T_MSVC] = "",
  [T_GNU]  = "-l",
};

const char* tc_lib_path_flag[] = {
  [T_MSVC] = "/LIBPATH:",
  [T_GNU]  = "-L",
};

//////////////////////////////////////////////////

#ifdef _WIN32
#define EXE_PATH(x) BIN_FOLDER x ".exe"
#else
#define EXE_PATH(x) BIN_FOLDER x 
#endif

typedef struct {
  Procs procs;
  Nob_File_Paths objs;
} walk_data_t;

static tc_t tc = T_MSVC;

const char* supported_tcs() {
  Nob_String_Builder sb = { 0 };

  nob_sb_appendf(&sb, "%s", tc_names[0]);
  for (size_t i = 1; i < COUNT_TOOLCHAINS; i++) {
    nob_sb_appendf(&sb, ", %s", tc_names[i]);
  }

  return nob_temp_sprintf("%.*s", (int)sb.count, sb.items);
}

void usage(FILE *stream, const char* exe)
{
  fprintf(stream, "Usage: ./%s [OPTIONS]\n", exe);
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

Nob_String_View filename_from_path(Nob_String_View path) {
  while (path.count > 0 && path.data[--path.count] != '.');

  Nob_String_View tmp = (Nob_String_View){
    .data = &path.data[path.count],
    .count = 0,
  };
  while(*(--tmp.data) != *OS_SEP && ++tmp.count <= path.count);
  tmp.data++;

  return tmp;
}

bool compile_tu(Nob_Walk_Entry entry) {
  Cmd cmd = {0};
  walk_data_t *wd = (walk_data_t*)entry.data;

  Nob_String_View path = nob_sv_from_cstr(entry.path);

  if (nob_sv_end_with(path, ".c")) {
    const char* obj_path = nob_temp_sprintf(BUILD_FOLDER"%.*s-%s.obj", SV_Arg(filename_from_path(path)), tc_names[tc]);

    nob_cmd_append(&cmd, tc_cc[tc]);
    for (size_t i=0; i < tc_lflags[tc].count; i++) {
      nob_cmd_append(&cmd, tc_cflags[tc].items[i]);
    }
    for (size_t i=0; i < tc_includes[tc].count; i++) {
      nob_cmd_append(&cmd, tc_include_flag[tc], tc_includes[tc].items[i]);
    }
    nob_cmd_append(&cmd, tc_compile_flag[tc], entry.path);
    nob_cmd_append(&cmd, tc_out_obj[tc], obj_path);

    nob_da_append(&wd->objs, obj_path);

    if (!cmd_run(&cmd, .async = &wd->procs)) return false;
  }

  return true;
}

#ifdef _WIN32
bool run_in_msvc_env(int argc, char** argv) {
  if(!GetEnvironmentVariable("VSCMD_VER", NULL, 0) && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
    nob_log(NOB_INFO, "Loading MSVC environment");
    Cmd cmd = { 0 };

    nob_cmd_append(
      &cmd, 
      "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", 
      "-arch=x64", 
      "-host_arch=x64", 
      ">", "NUL"
    );
    nob_cmd_append(&cmd, "&&");
    nob_da_append_many(&cmd, argv, argc);
    return cmd_run(&cmd);
  }
  return true;
}
#endif

int main(int argc, char **argv)
{
#if defined(_MSC_VER) && defined(_WIN32)
    const char *binary_path = *argv;

    if (!nob_sv_end_with(nob_sv_from_cstr(binary_path), ".exe")) {
      binary_path = nob_temp_sprintf("%s.exe", binary_path);
    }

    if(nob_needs_rebuild1(binary_path, __FILE__))
      if(!run_in_msvc_env(argc, argv)) return 1;
#endif
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool *f_help       = flag_bool("help", false, "Print this message to stdout and exit with 0.");
    char **f_toolchain = flag_str(
        "toolchain", 
        tc_names[0], 
        nob_temp_sprintf("Select the toolchain. Available toolchains: [%s]", supported_tcs())
    );
    bool *f_run = flag_bool(
      "run",
      false,
      "Launch executable after compilation."
    );

    const char* exe = argv[0];

    if (!flag_parse(argc, argv)) {
        usage(stderr, exe);
        flag_print_error(stderr);
        return 1; 
    }

    if (*f_help) {
        usage(stdout, exe);
        return 0;
    }

    for (; tc <= COUNT_TOOLCHAINS; tc++) {
      if (tc == COUNT_TOOLCHAINS) {
        nob_log(NOB_ERROR, "Invalid toolchain `%s`, Defaulting to %s", *f_toolchain, tc_names[0]);
        tc = 0;
        break;
      }
      if(strcmp(*f_toolchain, tc_names[tc]) == 0) break;
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    if (!mkdir_if_not_exists(BIN_FOLDER)) return 1;

    if(tc == T_MSVC) {
#ifdef _WIN32
      if(!run_in_msvc_env(argc, argv)) return 1;
#else
      // TODO: wine?
      nob_log(NOB_ERROR, "MSVC toolchain not available outside of Windows.");
      return 1;
#endif
    }

    argc = flag_rest_argc();
    argv = flag_rest_argv();

    uint64_t start = nob_nanos_since_unspecified_epoch();

    walk_data_t wd = { 0 };
    if(!nob_walk_dir(SRC_FOLDER, compile_tu, .data = &wd)) return 1;
    if (!procs_flush(&wd.procs)) return 1;

    Cmd cmd = { 0 };
    nob_cmd_append(&cmd, tc_cc[tc]);
    for (size_t i=0; i < tc_lflags[tc].count; i++) {
      nob_cmd_append(&cmd, tc_lflags[tc].items[i]);
    }
    da_foreach(const char*, obj, &wd.objs) {
      nob_da_append(&cmd, *obj); 
    }
    nob_cmd_append(&cmd, tc_out_exe[tc]);
    nob_cmd_append(&cmd, EXE_PATH(PROJ_NAME));
    for (size_t i=0; i < tc_lib_path[tc].count; i++) {
      nob_cmd_append(&cmd, nob_temp_sprintf("%s%s", tc_lib_path_flag[tc], tc_lib_path[tc].items[i]));
    }
    for (size_t i=0; i < tc_libs[tc].count; i++) {
      nob_cmd_append(&cmd, nob_temp_sprintf("%s%s", tc_lib_flag[tc], tc_libs[tc].items[i]));
    }

    if (!cmd_run(&cmd)) return 1;

    uint64_t end = nob_nanos_since_unspecified_epoch();
    uint64_t delta = end - start;

    nob_log(NOB_INFO, "Compilation completed in %.03lf seconds.", ((double)delta) / NOB_NANOS_PER_SEC);

    if (*f_run) {
      nob_cmd_append(&cmd, EXE_PATH(PROJ_NAME));
      nob_da_append_many(&cmd, argv, argc);
      if (!cmd_run(&cmd)) return 1;
    }

    return 0;
}
