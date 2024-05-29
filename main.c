#include "xcc.h"
#include <stdlib.h>
#include <curl/curl.h>

typedef enum {
  FILE_NONE, FILE_C, FILE_ASM, FILE_OBJ, FILE_AR, FILE_DSO,
} FileType;

StringArray include_paths;
int opt_O = 0;
bool opt_fcommon = true;
bool opt_run;
bool opt_asm;
bool opt_fpic;

char *opt_gccarg;
char *opt_rarg;

static FileType opt_x;
static StringArray opt_include;
static bool opt_E;
static bool opt_M;
static bool opt_MD;
static bool opt_MMD;
static bool opt_MP;
static bool opt_S;
static bool opt_c;
static bool opt_cc1;
static bool opt_gcc;
static bool opt_hash_hash_hash;
static bool opt_static;
static bool opt_shared;
static char *opt_MF;
static char *opt_MT;
static char *opt_o;

static StringArray ld_extra_args;
static StringArray std_include_paths;

char *base_file;
static char *output_file;

static StringArray input_paths;
static StringArray tmpfiles;

static void usage(int status) {
    fprintf(stderr, "XCompiler %s - a simple C compiler\n", VERSION);
    fprintf(stderr, "Usage: xc [options] file...\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "Compilation Options:\n");
    fprintf(stderr, "  %-20s %s\n", "-o <file>", "Output to specified file");
    fprintf(stderr, "  %-20s %s\n", "-S", "Compile only; do not assemble or link");
    fprintf(stderr, "  %-20s %s\n", "-c", "Compile source files without linking");
    fprintf(stderr, "  %-20s %s\n", "-E", "Preprocess only; do not compile, assemble, or link");
    fprintf(stderr, "  %-20s %s\n", "-I<path>", "Add directory to the include search path");
    fprintf(stderr, "  %-20s %s\n", "-D<macro>", "Define macro");
    fprintf(stderr, "  %-20s %s\n", "-U<macro>", "Undefine macro");
    fprintf(stderr, "  %-20s %s\n", "-include <file>", "Process file as if included");
    fprintf(stderr, "  %-20s %s\n", "-x <language>", "Specify source language");
    fprintf(stderr, "  %-20s %s\n", "-O", "Compile and Optimize using gcc backend");
    fprintf(stderr, "  %-20s %s\n", "-fpic", "Generate position-independent code (PIC)");
    fprintf(stderr, "  %-20s %s\n", "-fPIC", "Generate PIC suitable for use in shared libraries");
    fprintf(stderr, "  %-20s %s\n", "-idirafter <path>", "Add directory to include search path after system directories");
    fprintf(stderr, "  %-20s %s\n", "-static", "Link statically");
    fprintf(stderr, "  %-20s %s\n", "-shared", "Create shared object");
    fprintf(stderr, "  %-20s %s\n", "-L <path>", "Add directory to the library search path");
    fprintf(stderr, "  %-20s %s\n", "--help", "Display help message");
    fprintf(stderr, "\nLinking Options:\n");
    fprintf(stderr, "  %-20s %s\n", "-l<library>", "Link with specified library");
    fprintf(stderr, "  %-20s %s\n", "-Wl,<option>", "Pass option to the linker");
    fprintf(stderr, "  %-20s %s\n", "-Xlinker <option>", "Pass option to the linker");
    fprintf(stderr, "\nDependency Generation Options:\n");
    fprintf(stderr, "  %-20s %s\n", "-M", "Generate Makefile-style dependency");
    fprintf(stderr, "  %-20s %s\n", "-MF <file>", "Write dependency information to specified file");
    fprintf(stderr, "  %-20s %s\n", "-MP", "Add phony target for each dependency");
    fprintf(stderr, "  %-20s %s\n", "-MT <target>", "Specify target for dependency");
    fprintf(stderr, "  %-20s %s\n", "-MD", "Write dependency information and compile");
    fprintf(stderr, "  %-20s %s\n", "-MQ <target>", "Specify target for dependency");
    fprintf(stderr, "  %-20s %s\n", "-MMD", "Write dependency information and compile if necessary");
    fprintf(stderr, "\nOther Options:\n");
    fprintf(stderr, "  %-20s %s\n", "-run", "Run the program");
    //fprintf(stderr, "  %-20s %s\n", "-rarg", "Arguments of the runned program."); // built in used in xc bash shebang fixer
    fprintf(stderr, "  %-20s %s\n", "-asm", "Print generated asm");
    fprintf(stderr, "  %-20s %s\n", "-gcc", "Use gcc backend to compile.");
    fprintf(stderr, "  %-20s %s\n", "-gccarg", "Specify gcc args to compile.");
    fprintf(stderr, "  %-20s %s\n", "-###", "Print commands executed by the compiler");
    fprintf(stderr, "  %-20s %s\n", "-cc1", "Invoke the C compiler driver");
    fprintf(stderr, "  %-20s %s\n", "-fcommon", "Allow multiple definitions of a global variable");
    fprintf(stderr, "  %-20s %s\n", "-fno-common", "Do not allow multiple definitions of a global variable");
    fprintf(stderr, "  %-20s %s\n", "-s", "Remove all symbol table and relocation information");
    fprintf(stderr, "  %-20s %s\n", "-cc1-input <file>", "Specify input file for C compiler driver");
    fprintf(stderr, "  %-20s %s\n", "-cc1-output <file>", "Specify output file for C compiler driver");
    fprintf(stderr, "For more information, see the documentation.\n");
    exit(status);
}

static void version() {
  fprintf(stderr, "xc %s\n", VERSION);
  exit(0);
}

static bool take_arg(char *arg) {
  char *x[] = {
     "-O", "-o", "-I", "-idirafter", "-include", "-x", "-MF", "-MT", "-Xlinker", "-soname", "-gccarg", "-rarg"
  };
  for (int i = 0; i < sizeof(x) / sizeof(*x); i++)
    if (!strcmp(arg, x[i])) {
      return true;
    } else {
      return false;
    }
  return false;
}

static void add_default_include_paths(char *argv0) {
  // We expect that xcc-specific include files are installed
  // to ./include relative to argv[0].
  strarray_push(&include_paths, format("%s/include", dirname(strdup(argv0))));

  // Add standard include paths.

  strarray_push(&include_paths, "/usr/local/include/xcc/");
  strarray_push(&include_paths, "/usr/local/include");
  strarray_push(&include_paths, "/usr/include/x86_64-linux-gnu");
  
  strarray_push(&include_paths, "/usr/include");
  strarray_push(&include_paths, "/usr/include/linux");

  // Keep a copy of the standard include paths for -MMD option.
  for (int i = 0; i < include_paths.len; i++)
    strarray_push(&std_include_paths, include_paths.data[i]);
}

static void define(char *str) {
  char *eq = strchr(str, '=');
  if (eq)
    define_macro(strndup(str, eq - str), eq + 1);
  else
    define_macro(str, "1");
}

static FileType parse_opt_x(char *s) {
  if (!strcmp(s, "c"))
    return FILE_C;
  if (!strcmp(s, "assembler"))
    return FILE_ASM;
  if (!strcmp(s, "none"))
    return FILE_NONE;
  error("<command line>: unknown argument for -x: %s", s);
}

static char *quote_makefile(char *s) {
  char *buf = calloc(1, strlen(s) * 2 + 1);

  for (int i = 0, j = 0; s[i]; i++) {
    switch (s[i]) {
    case '$':
      buf[j++] = '$';
      buf[j++] = '$';
      break;
    case '#':
      buf[j++] = '\\';
      buf[j++] = '#';
      break;
    case ' ':
    case '\t':
      for (int k = i - 1; k >= 0 && s[k] == '\\'; k--)
        buf[j++] = '\\';
      buf[j++] = '\\';
      buf[j++] = s[i];
      break;
    default:
      buf[j++] = s[i];
      break;
    }
  }
  return buf;
}

static void parse_args(int argc, char **argv) {
  // Make sure that all command line options that take an argument
  // have an argument.
  for (int i = 1; i < argc; i++)
    if (take_arg(argv[i]))
      if (!argv[++i])
        usage(1);

  StringArray idirafter = {};

  for (int i = 1; i < argc; i++) {

    if (!strcmp(argv[i], "-###")) {
      opt_hash_hash_hash = true;
      continue;
    }

    if (!strcmp(argv[i], "-run")) {
      opt_run = true;
      continue;
    }

    if (!strcmp(argv[i], "-asm")) {
      opt_asm = true;
      continue;
    }

    if (!strcmp(argv[i], "-gcc")) {
      opt_gcc = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1")) {
      opt_cc1 = true;
      continue;
    }

    if (!strcmp(argv[i], "--help")||!strcmp(argv[i], "-h"))
      usage(0);


    if (!strcmp(argv[i], "--version"))
      version();

    if (!strcmp(argv[i], "-o")) {
      opt_o = argv[++i];
      continue;
    }

    if (!strncmp(argv[i], "-o", 2)) {
      opt_o = argv[i] + 2;
      continue;
    }

    if (!strncmp(argv[i], "-Os", 3)) {
        opt_O = -1;
        continue;
    }

    if (!strncmp(argv[i], "-O", 2)) {
        int level = argv[i][2] - '0';
        if (level >= 0 && level <= 3) {
            opt_O = level;
        } 
        continue;
    }

    if (!strcmp(argv[i], "-soname")) {
      strarray_push(&ld_extra_args, argv[i]);
      strarray_push(&ld_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-S")) {
      opt_S = true;
      continue;
    }

    if (!strcmp(argv[i], "-fcommon")) {
      opt_fcommon = true;
      continue;
    }

    if (!strcmp(argv[i], "-fno-common")) {
      opt_fcommon = false;
      continue;
    }

    if (!strcmp(argv[i], "-c")) {
      opt_c = true;
      continue;
    }

    if (!strcmp(argv[i], "-E")) {
      opt_E = true;
      continue;
    }

    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(&include_paths, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-D")) {
      define(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-D", 2)) {
      define(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-U")) {
      undef_macro(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-U", 2)) {
      undef_macro(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-include")) {
      strarray_push(&opt_include, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-x")) {
      opt_x = parse_opt_x(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-x", 2)) {
      opt_x = parse_opt_x(argv[i] + 2);
      continue;
    }

    if (!strncmp(argv[i], "-l", 2) || !strncmp(argv[i], "-Wl,", 4)) {
      strarray_push(&input_paths, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-Xlinker")) {
      strarray_push(&ld_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-s")) {
      strarray_push(&ld_extra_args, "-s");
      continue;
    }

    if (!strcmp(argv[i], "-M")) {
      opt_M = true;
      continue;
    }

    if (!strcmp(argv[i], "-MF")) {
      opt_MF = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-MP")) {
      opt_MP = true;
      continue;
    }

    if (!strcmp(argv[i], "-MT")) {
      if (opt_MT == NULL)
        opt_MT = argv[++i];
      else
        opt_MT = format("%s %s", opt_MT, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-MD")) {
      opt_MD = true;
      continue;
    }

    if (!strcmp(argv[i], "-MQ")) {
      if (opt_MT == NULL)
        opt_MT = quote_makefile(argv[++i]);
      else
        opt_MT = format("%s %s", opt_MT, quote_makefile(argv[++i]));
      continue;
    }

    if (!strcmp(argv[i], "-MMD")) {
      opt_MD = opt_MMD = true;
      continue;
    }

    if (!strcmp(argv[i], "-fpic") || !strcmp(argv[i], "-fPIC")) {
      opt_fpic = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1-input")) {
      base_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-output")) {
      output_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-idirafter")) {
      strarray_push(&idirafter, argv[i++]);
      continue;
    }

    if (!strcmp(argv[i], "-gccarg")) {
      opt_gcc = true;
      opt_gccarg = argv[++i];
      continue;
    }
    
    if (!strcmp(argv[i], "-rarg")) {
      opt_rarg = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-static")) {
      opt_static = true;
      strarray_push(&ld_extra_args, "-static");
      continue;
    }

    if (!strcmp(argv[i], "-shared")) {
      opt_shared = true;
      strarray_push(&ld_extra_args, "-shared");
      continue;
    }

    if (!strcmp(argv[i], "-L")) {
      strarray_push(&ld_extra_args, "-L");
      strarray_push(&ld_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-L", 2)) {
      strarray_push(&ld_extra_args, "-L");
      strarray_push(&ld_extra_args, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-hashmap-test")) {
      hashmap_test();
      exit(0);
    }

    // These options are ignored for now.
    if (!strncmp(argv[i], "-W", 2) ||
        !strncmp(argv[i], "-g", 2) ||
        !strncmp(argv[i], "-std=", 5) ||
        !strcmp(argv[i], "-ffreestanding") ||
        !strcmp(argv[i], "-fno-builtin") ||
        !strcmp(argv[i], "-fno-omit-frame-pointer") ||
        !strcmp(argv[i], "-fno-stack-protector") ||
        !strcmp(argv[i], "-fno-strict-aliasing") ||
        !strcmp(argv[i], "-m64") ||
        !strcmp(argv[i], "-mno-red-zone") ||
        !strcmp(argv[i], "-w"))
      continue;

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(&input_paths, argv[i]);
  }

  for (int i = 0; i < idirafter.len; i++)
    strarray_push(&include_paths, idirafter.data[i]);

  if (input_paths.len == 0) {
    printf("Please input file to use it\n");
    usage(1);
  }

  // -E implies that the input is the C macro language.
  if (opt_E)
    opt_x = FILE_C;
}

static FILE *open_file(char *path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;

  FILE *out = fopen(path, "w");
  if (!out)
    error("cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

static bool endswith(char *p, char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

// Replace file extension
static char *replace_extn(char *tmpl, char *extn) {
  char *filename = basename(strdup(tmpl));
  char *dot = strrchr(filename, '.');
  if (dot)
    *dot = '\0';
  return format("%s%s", filename, extn);
}

static void cleanup(void) {
  for (int i = 0; i < tmpfiles.len; i++)
    unlink(tmpfiles.data[i]);
}

static char *create_tmpfile(void) {
  char *path = strdup("/tmp/xcc-XXXXXX");
  int fd = mkstemp(path);
  if (fd == -1)
    error("mkstemp failed: %s", strerror(errno));
  close(fd);

  strarray_push(&tmpfiles, path);
  return path;
}

static void run_subprocess(char **argv) {
  
  // If -### is given, dump the subprocess's command line.
  if (opt_hash_hash_hash) {
    fprintf(stderr, "%s", argv[0]);
    for (int i = 1; argv[i]; i++)
      fprintf(stderr, " %s", argv[i]);
    fprintf(stderr, "\n");
  }

  if (fork() == 0) {
    // Child process. Run a new command.
    execvp(argv[0], argv);
    fprintf(stderr, "exec failed: %s: %s\n", argv[0], strerror(errno));
    _exit(1);
  }

  // Wait for the child process to finish.
  int status;
  while (wait(&status) > 0);
  if (status != 0)
    exit(1);
}

static void run_cc1(int argc, char **argv, char *input, char *output) {
  char **args = calloc(argc + 10, sizeof(char *));
  memcpy(args, argv, argc * sizeof(char *));
  args[argc++] = "-cc1";

  if (input) {
    args[argc++] = "-cc1-input";
    args[argc++] = input;
  }

  if (output) {
    args[argc++] = "-cc1-output";
    args[argc++] = output;
  }

  run_subprocess(args);
}

// Print tokens to stdout. Used for -E.
static void print_tokens(Token *tok) {
  FILE *out = open_file(opt_o ? opt_o : "-");

  int line = 1;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (line > 1 && tok->at_bol)
      fprintf(out, "\n");
    if (tok->has_space && !tok->at_bol)
      fprintf(out, " ");
    fprintf(out, "%.*s", tok->len, tok->loc);
    line++;
  }
  fprintf(out, "\n");
}

static bool in_std_include_path(char *path) {
  for (int i = 0; i < std_include_paths.len; i++) {
    char *dir = std_include_paths.data[i];
    int len = strlen(dir);
    if (strncmp(dir, path, len) == 0 && path[len] == '/')
      return true;
  }
  return false;
}

// If -M options is given, the compiler write a list of input files to
// stdout in a format that "make" command can read. This feature is
// used to automate file dependency management.
static void print_dependencies(void) {
  char *path;
  if (opt_MF)
    path = opt_MF;
  else if (opt_MD)
    path = replace_extn(opt_o ? opt_o : base_file, ".d");
  else if (opt_o)
    path = opt_o;
  else
    path = "-";

  FILE *out = open_file(path);
  if (opt_MT)
    fprintf(out, "%s:", opt_MT);
  else
    fprintf(out, "%s:", quote_makefile(replace_extn(base_file, ".o")));

  File **files = get_input_files();

  for (int i = 0; files[i]; i++) {
    if (opt_MMD && in_std_include_path(files[i]->name))
      continue;
    fprintf(out, " \\\n  %s", files[i]->name);
  }

  fprintf(out, "\n\n");

  if (opt_MP) {
    for (int i = 1; files[i]; i++) {
      if (opt_MMD && in_std_include_path(files[i]->name))
        continue;
      fprintf(out, "%s:\n\n", quote_makefile(files[i]->name));
    }
  }
}

static Token *must_tokenize_file(char *path) {
  Token *tok = tokenize_file(path);
  if (!tok)
    error("%s: %s", path, strerror(errno));
  return tok;
}

static Token *append_tokens(Token *tok1, Token *tok2) {
  if (!tok1 || tok1->kind == TK_EOF)
    return tok2;

  Token *t = tok1;
  while (t->next->kind != TK_EOF)
    t = t->next;
  t->next = tok2;
  return tok1;
}

#define MAX_URL_LENGTH 2048

#define PATH_MAX 4096 // Define the maximum path length

char *get_launch_directory() {
    char *path = malloc(PATH_MAX);
    if (path == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    if (getcwd(path, PATH_MAX) == NULL) {
        perror("getcwd() error");
        free(path);
        exit(EXIT_FAILURE);
    }

    return path;
}

char* process_gcc(const char *base_file) {
    FILE *input = fopen(base_file, "r");
    if (input == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", base_file);
        exit(EXIT_FAILURE);
    }
    
    // Read the first line
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), input) == NULL) {
        fclose(input);
        return strdup(base_file);
    }
    
    // Check if the first line starts with "#!"
   if (strncmp(buffer, "#!", 2) == 0) {
        if (fgets(buffer, sizeof(buffer), input) == NULL) {
            rewind(input);
        }
    } else {
        rewind(input);
    }

    // Create a temporary file
    char temp_file[] = "/tmp/ccx-gccXXXXXX";
    int temp_fd = mkstemp(temp_file);
    if (temp_fd == -1) {
        fclose(input);
        exit(EXIT_FAILURE);
    }
    FILE *temp = fdopen(temp_fd, "w");
    if (temp == NULL) {
        fclose(input);
        close(temp_fd);
        exit(EXIT_FAILURE);
    }

    // Write the modified content to the temporary file
    fputs(buffer, temp); // Write the first line
    while (fgets(buffer, sizeof(buffer), input) != NULL) {
        if (strncmp(buffer, "#include \"https://", 17) == 0) {
            // Extract the URL from the line
            char *url_start = strchr(buffer, '"') + 1;
            char *url_end = strchr(url_start, '"');
            size_t url_len = url_end - url_start;

            // Copy the URL into a string
            char url[MAX_URL_LENGTH];
            strncpy(url, url_start, url_len);
            url[url_len] = '\0';

            // Extract filename from URL
            char *filename_start = strrchr(url, '/') + 1;
            if (filename_start == NULL) {
                error("Error: Unable to extract filename from URL");
            }

            // Construct the string
            char include_string[MAX_URL_LENGTH * 2]; // Maximum size of dirname + filename_start
            snprintf(include_string, sizeof(include_string), "%s/%s", get_launch_directory(), filename_start);

            // Check if the file is already downloaded
            if (!file_exists(include_string)) {
                CURL *curl = curl_easy_init();
                if (curl == NULL) {
                    error("Could not initialize cURL context");
                }

                FILE *download_file = fopen(include_string, "w");
                if (download_file == NULL) {
                    fprintf(stderr, "Could not open file '%s'\n", include_string);
                }

                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, download_file);
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    fprintf(stderr, "Could not download '%s': %s\n", url, curl_easy_strerror(res));
                }

                curl_easy_cleanup(curl);
                fclose(download_file);
            }

            char include_buffer[1024];
            snprintf(include_buffer, sizeof(include_buffer), "#include \"%s/%s\"\n", get_launch_directory(), filename_start);
            fputs(include_buffer, temp);
        } else {
            fputs(buffer, temp);
        }
    }
    
    fclose(input);
    fclose(temp);
    return strdup(temp_file);
}

static void cc1(void) {
    if (opt_O > 0 || opt_gcc) {
        // Open a temporary output buffer
        char *buf;
        size_t buflen;
        FILE *output_buf = open_memstream(&buf, &buflen);
        if (output_buf == NULL) {
            error("Failed to open memory stream");
        }

        // Use popen to execute GCC and redirect its output to our buffer
        char cmd[4096];
        char include_cmd[1024];
        for (int i = 0; i < include_paths.len; i++) {
            if (strcmp(include_paths.data[i], "/usr/local/bin/xcc/include") == 0 || strcmp(include_paths.data[i], "/usr/local/include/xcc/") == 0 || strcmp(include_paths.data[i], "/usr/local/include") == 0 || strcmp(include_paths.data[i], "/usr/include/x86_64-linux-gnu") == 0 || strcmp(include_paths.data[i], "/usr/include") == 0 || strcmp(include_paths.data[i], "/usr/include/linux") == 0) {
                continue;
            }
            strcat(include_cmd, "-I");
            strcat(include_cmd, include_paths.data[i]);
            strcat(include_cmd, " ");
        }

        char *tmpfile = process_gcc(base_file);
        if(!file_exists(tmpfile)){
          error("Failed to gen tmp gcc c file.");
        }


        if(opt_gccarg == NULL){
          if (opt_O == -1) {
            snprintf(cmd, sizeof(cmd), "gcc %s-S -Os -o \"%s\" -x c \"%s\"", include_cmd, output_file, tmpfile);
          } else {
            snprintf(cmd, sizeof(cmd), "gcc %s-S -O%d -o \"%s\" -x c \"%s\"", include_cmd, opt_O, output_file, tmpfile);
          }
        } else {
          if (opt_O == -1) {
            snprintf(cmd, sizeof(cmd), "gcc %s %s-S -Os -o \"%s\" -x c \"%s\"", opt_gccarg, include_cmd, output_file, tmpfile);
          } else {
            snprintf(cmd, sizeof(cmd), "gcc %s %s-S -O%d -o \"%s\" -x c \"%s\"", opt_gccarg, include_cmd, opt_O, output_file, tmpfile);
          }
        }

        printf("%s\n",cmd);
        
        FILE *gcc_output = popen(cmd, "r");
        if (gcc_output == NULL || !file_exists(tmpfile)) {
            printf("%s\n",cmd);
            error("Failed to execute GCC");
        }

        // Read the output of GCC and write it to the output buffer
        char buffer[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), gcc_output)) > 0) {
            fwrite(buffer, 1, bytes_read, output_buf);
        }

        // Close the pipes and free resources
        pclose(gcc_output);
        fclose(output_buf);
    } else {
        Token *tok = NULL;

        // Process -include option
        for (int i = 0; i < opt_include.len; i++) {
            char *incl = opt_include.data[i];

            char *path;
            if (file_exists(incl)) {
                path = incl;
            } else {
                path = search_include_paths(incl);
                if (!path)
                    error("-include: %s: %s", incl, strerror(errno));
            }

            Token *tok2 = must_tokenize_file(path);
            tok = append_tokens(tok, tok2);
        }

        // Tokenize and parse.
        Token *tok2 = must_tokenize_file(base_file);
        tok = append_tokens(tok, tok2);
        tok = preprocess(tok);

        // If -M or -MD are given, print file dependencies.
        if (opt_M || opt_MD) {
            print_dependencies();
            if (opt_M)
                return;
        }

        // If -E is given, print out preprocessed C code as a result.
        if (opt_E) {
            print_tokens(tok);
            return;
        }

        Obj *prog = parse(tok);

        // Open a temporary output buffer
        char *buf;
        size_t buflen;
        FILE *output_buf = open_memstream(&buf, &buflen);
        if (output_buf == NULL) {
            error("Failed to open memory stream");
        }

        // Traverse the AST to emit assembly.
        codegen(prog, output_buf);
        fclose(output_buf);

        // Write the assembly text to a file.
        FILE *out = open_file(output_file);
        fwrite(buf, buflen, 1, out);
        fclose(out);
    }
}

static void assemble(char *input, char *output) {
  char *cmd[] = {"as", "-c", input, "-o", output, NULL};
  run_subprocess(cmd);
}

static char *find_file(char *pattern) {
  char *path = NULL;
  glob_t buf = {};
  glob(pattern, 0, NULL, &buf);
  if (buf.gl_pathc > 0)
    path = strdup(buf.gl_pathv[buf.gl_pathc - 1]);
  globfree(&buf);
  return path;
}

// Returns true if a given file exists.
bool file_exists(char *path) {
  struct stat st;
  return !stat(path, &st);
}

static char *find_libpath(void) {
  if (file_exists("/usr/lib/x86_64-linux-gnu/crti.o"))
    return "/usr/lib/x86_64-linux-gnu";
  if (file_exists("/usr/lib64/crti.o"))
    return "/usr/lib64";
  error("library path is not found");
}

static char *find_gcc_libpath(void) {
  char *paths[] = {
    "/usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o",
    "/usr/lib/gcc/x86_64-pc-linux-gnu/*/crtbegin.o", // For Gentoo
    "/usr/lib/gcc/x86_64-redhat-linux/*/crtbegin.o", // For Fedora
  };

  for (int i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    char *path = find_file(paths[i]);
    if (path)
      return dirname(path);
  }

  error("gcc library path is not found");
}

static void run_linker(StringArray *inputs, char *output) {

  StringArray arr = {};

  strarray_push(&arr, "ld");
  strarray_push(&arr, "-o");
  strarray_push(&arr, output);
  strarray_push(&arr, "-m");
  strarray_push(&arr, "elf_x86_64");

  char *libpath = find_libpath();
  char *gcc_libpath = find_gcc_libpath();

  if (opt_shared) {
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbeginS.o", gcc_libpath));
  } else {
    strarray_push(&arr, format("%s/crt1.o", libpath));
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbegin.o", gcc_libpath));
  }

  strarray_push(&arr, format("-L%s", gcc_libpath));
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib64");
  strarray_push(&arr, "-L/lib64");

  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-pc-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-redhat-linux");
  strarray_push(&arr, "-L/usr/lib");
  strarray_push(&arr, "-L/lib");

  if (!opt_static) {
    strarray_push(&arr, "-dynamic-linker");
    strarray_push(&arr, "/lib64/ld-linux-x86-64.so.2");
  }

  for (int i = 0; i < ld_extra_args.len; i++)
    strarray_push(&arr, ld_extra_args.data[i]);

  for (int i = 0; i < inputs->len; i++)
    strarray_push(&arr, inputs->data[i]);

  if (opt_static) {
    strarray_push(&arr, "--start-group");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "-lgcc_eh");
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "--end-group");
  } else {
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "--as-needed");
    strarray_push(&arr, "-lgcc_s");
    strarray_push(&arr, "--no-as-needed");
  }

  if (opt_shared)
    strarray_push(&arr, format("%s/crtendS.o", gcc_libpath));
  else
    strarray_push(&arr, format("%s/crtend.o", gcc_libpath));

  strarray_push(&arr, format("%s/crtn.o", libpath));
  strarray_push(&arr, NULL);

  run_subprocess(arr.data);
}

static FileType get_file_type(char *filename) {
  if (opt_x != FILE_NONE)
    return opt_x;
  if (endswith(filename, ".a"))
    return FILE_AR;
  if (endswith(filename, ".so"))
    return FILE_DSO;
  if (endswith(filename, ".o"))
    return FILE_OBJ;
  if (endswith(filename, ".c") || endswith(filename, ""))
    return FILE_C;
  if (endswith(filename, ".s") || endswith(filename, ".S") || endswith(filename, ".asm"))
    return FILE_ASM;
  if(opt_run){
    return FILE_C;
  } 
  error("<command line>: unknown file extension: %s", filename);
}

void executeIfExists(char *file, int argc, char **argv) {
    if (file_exists(file)) {
        char exec[256];
        char rm[100];
        snprintf(exec, sizeof(exec), "./%s", file);
        //printf("\nArgs: %s\n",opt_rarg);
        if (opt_rarg != NULL) {
            strcat(exec, " ");
            strcat(exec, opt_rarg);
        }
        system(exec);
        snprintf(rm, sizeof(rm), "rm -rf ./%s", file);
        system(rm);
    }
}

int main(int argc, char **argv) {
  atexit(cleanup);
  init_macros();
  parse_args(argc, argv);

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    error("Could not initialize Global cURL context.");
  }

  if (opt_cc1) {
    add_default_include_paths(argv[0]);
    cc1();
    return 0;
  }

  if (input_paths.len > 1 && opt_o && (opt_c || opt_S || opt_E))
    error("cannot specify '-o' with '-c,' '-S' or '-E' with multiple files");

  StringArray ld_args = {};

  for (int i = 0; i < input_paths.len; i++) {
    char *input = input_paths.data[i];

    if (!strncmp(input, "-l", 2)) {
      strarray_push(&ld_args, input);
      continue;
    }

    if (!strncmp(input, "-Wl,", 4)) {
      char *s = strdup(input + 4);
      char *arg = strtok(s, ",");
      while (arg) {
        strarray_push(&ld_args, arg);
        arg = strtok(NULL, ",");
      }
      continue;
    }

    char *output;
    if (opt_o)
      output = opt_o;
    else if (opt_S)
      output = replace_extn(input, ".s");
    else
      output = replace_extn(input, ".o");

    FileType type = get_file_type(input);

    // Handle .o or .a
    if (type == FILE_OBJ || type == FILE_AR || type == FILE_DSO) {
      strarray_push(&ld_args, input);
      continue;
    }

    // Handle .s, -S, .asm
    if (type == FILE_ASM) {
      if (!opt_S)
        assemble(input, output);
      continue;
    }

    assert(type == FILE_C);

    // Just preprocess
    if (opt_E || opt_M) {
      run_cc1(argc, argv, input, NULL);
      continue;
    }

    // Compile
    if (opt_S) {
      run_cc1(argc, argv, input, output);
      continue;
    }

    // Compile and assemble
    if (opt_c) {
      char *tmp = create_tmpfile();
      run_cc1(argc, argv, input, tmp);
      assemble(tmp, output);
      continue;
    }

    // Compile, assemble and link
    char *tmp1 = create_tmpfile();
    char *tmp2 = create_tmpfile();
    run_cc1(argc, argv, input, tmp1);
    assemble(tmp1, tmp2);
    strarray_push(&ld_args, tmp2);
    continue;
  }

  curl_global_cleanup();

  if(opt_run) {
    char *output_file_elf = opt_o ? opt_o : "a.out";
    run_linker(&ld_args, output_file_elf);
    executeIfExists(output_file_elf,argc,argv);
  } else if (ld_args.len > 0) {
    run_linker(&ld_args, opt_o ? opt_o : "a.out");
  }

  return 0;
}
