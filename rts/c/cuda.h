/* Simple CUDA runtime framework */

#define CUDA_SUCCEED(x) cuda_api_succeed(x, #x, __FILE__, __LINE__)
#define NVRTC_SUCCEED(x) nvrtc_api_succeed(x, #x, __FILE__, __LINE__)

static inline void cuda_api_succeed(CUresult res, const char *call,
    const char *file, int line)
{
  if (res != CUDA_SUCCESS) {
    const char *err_str;
    cuGetErrorString(res, &err_str);
    if (err_str == NULL) { err_str = "Unknown"; }
    panic(-1, "%s:%d: CUDA call\n  %s\nfailed with error code %d (%s)\n",
        file, line, call, res, err_str);
  }
}

static inline void nvrtc_api_succeed(nvrtcResult res, const char *call,
    const char *file, int line)
{
  if (res != NVRTC_SUCCESS) {
    const char *err_str = nvrtcGetErrorString(res);
    panic(-1, "%s:%d: NVRTC call\n  %s\nfailed with error code %d (%s)\n",
        file, line, call, res, err_str);
  }
}

struct cuda_config {
  int debugging;
  int logging;
  const char *preferred_device;

  const char *dump_program_to;
  const char *load_program_from;

  const char *dump_ptx_to;
  const char *load_ptx_from;

  size_t default_block_size;
  size_t default_grid_size;
  size_t default_tile_size;
  size_t default_threshold;
  size_t default_num_nodes;

  int default_block_size_changed;
  int default_grid_size_changed;
  int default_tile_size_changed;

  int num_sizes;
  const char **size_names;
  const char **size_vars;
  size_t *size_values;
  const char **size_classes;
};

void cuda_config_init(struct cuda_config *cfg,
                      int num_sizes,
                      const char *size_names[],
                      const char *size_vars[],
                      size_t *size_values,
                      const char *size_classes[])
{
  cfg->debugging = 0;
  cfg->logging = 0;
  cfg->preferred_device = "";

  cfg->dump_program_to = NULL;
  cfg->load_program_from = NULL;

  cfg->dump_ptx_to = NULL;
  cfg->load_ptx_from = NULL;

  cfg->default_block_size = 256;
  cfg->default_grid_size = 128;
  cfg->default_tile_size = 32;
  cfg->default_threshold = 32*1024;
  cfg->default_num_nodes = 1;

  cfg->default_block_size_changed = 0;
  cfg->default_grid_size_changed = 0;
  cfg->default_tile_size_changed = 0;

  cfg->num_sizes = num_sizes;
  cfg->size_names = size_names;
  cfg->size_vars = size_vars;
  cfg->size_values = size_values;
  cfg->size_classes = size_classes;
}

struct cuda_node_context {
  CUdevice dev;
  CUcontext cu_ctx;

  struct free_list free_list;
};

struct cuda_context {
  struct cuda_node_context *nodes;
  
  CUmodule module;

  size_t max_block_size;
  size_t max_grid_size;
  size_t max_tile_size;
  size_t max_threshold;
  size_t max_num_nodes;

  size_t lockstep_width;

  struct cuda_config cfg;
};

#define CU_DEV_ATTR(x) (CU_DEVICE_ATTRIBUTE_##x)
#define device_query(dev,attrib) _device_query(dev, CU_DEV_ATTR(attrib))
static int _device_query(CUdevice dev, CUdevice_attribute attrib)
{
  int val;
  CUDA_SUCCEED(cuDeviceGetAttribute(&val, attrib, dev));
  return val;
}

#define CU_FUN_ATTR(x) (CU_FUNC_ATTRIBUTE_##x)
#define function_query(fn,attrib) _function_query(dev, CU_FUN_ATTR(attrib))
static int _function_query(CUfunction dev, CUfunction_attribute attrib)
{
  int val;
  CUDA_SUCCEED(cuFuncGetAttribute(&val, attrib, dev));
  return val;
}

void set_preferred_device(struct cuda_config *cfg, const char *s)
{
  cfg->preferred_device = s;
}

struct cuda_device_entry {
  CUdevice dev;
  char name[256];
  int cc_major, cc_minor, id;
};

static int cuda_devices_setup(struct cuda_context *ctx)
{
  int count;
  int contains_valid = 0;

  ctx->max_num_nodes = ctx->cfg.default_num_nodes;

  CUDA_SUCCEED(cuDeviceGetCount(&count));
  if (count == 0) {
    panic(-1, "No suitable CUDA device found.\n");
  }

  struct cuda_device_entry *devs = calloc(count, sizeof(struct cuda_device_entry));
  
  for (int i = 0; i < count; ++i) {
    devs[i].id = i;
    CUDA_SUCCEED(cuDeviceGet(&devs[i].dev, i));

    if (device_query(devs[i].dev, COMPUTE_MODE) == CU_COMPUTEMODE_PROHIBITED) {
      if (ctx->cfg.debugging) {
        fprintf(stderr, "Device #%d is compute-prohibited, ignoring\n", i);
      }
      continue;
    }

    devs[i].cc_major = device_query(devs[i].dev, COMPUTE_CAPABILITY_MAJOR);
    devs[i].cc_minor = device_query(devs[i].dev, COMPUTE_CAPABILITY_MINOR);
    
    CUDA_SUCCEED(cuDeviceGetName(devs[i].name, sizeof(devs[i].name)/sizeof(devs[i].name[0]) - 1,
                                 devs[i].dev));
    // ^ NULL-terminated by calloc

    if (ctx->cfg.debugging) {
      fprintf(stderr, "Device #%d: name=\"%s\", compute capability=%d.%d\n",
          i, devs[i].name, devs[i].cc_major, devs[i].cc_minor);
    }

    contains_valid = 1;
  }

  if (!contains_valid) {
    panic(-1, "No compute-enabled devices found.");
  }

  int dev_count = 0;
  if (strcmp(ctx->cfg.preferred_device, "")) {
    // Sort by compute capability.
    for (int i = 0; i < count; ++i) {
      for (int j = 0; j < count; ++j) {
        int lower_cc = devs[j].cc_major < devs[i].cc_major ||
                       devs[j].cc_minor < devs[i].cc_minor;
        int same_cc = devs[j].cc_major == devs[i].cc_major &&
                      devs[j].cc_minor == devs[i].cc_minor;
        if (lower_cc || same_cc && strcmp(devs[j].name, devs[i].name) < 0) {
          struct cuda_device_entry temp = devs[i];
          devs[i] = devs[j];
          devs[j] = temp;
        }
      }
    }

    // XXX: Selecting devices with highest compute capability. Is there a better way?
    dev_count = 1;
    for (; dev_count < count && strcmp(devs[0].name, devs[dev_count].name); ++dev_count) {}
  } else {
    // Place all devices matching the preferred device in front of the list.
    for (int i = 0; i < count; ++i) {
      if (strstr(devs[i].name, ctx->cfg.preferred_device) == devs[i].name) {
        devs[dev_count++] = devs[i];
      }
    }
  }

  if (ctx->max_num_nodes != 0 && dev_count < ctx->max_num_nodes) {
    panic(-1, "Found only %d \"%s\" devices, but %d was requested.\n",
          dev_count, devs[0].name, ctx->max_num_nodes);
  }

  if (ctx->max_num_nodes == 0)
    ctx->cfg.default_num_nodes = ctx->max_num_nodes = dev_count;
  ctx->nodes = malloc(sizeof(struct cuda_node_context) * ctx->max_num_nodes);

  for (int i = 0; i < ctx->max_num_nodes; ++i)
    ctx->nodes[i].dev = devs[i].dev;

  if(ctx->cfg.debugging)
    fprintf(stderr, "Using %d \"%s\" devices.\n", ctx->max_num_nodes, devs[0].name);

  free(devs);
  return 0;
}

static char *concat_fragments(const char *src_fragments[])
{
  size_t src_len = 0;
  const char **p;

  for (p = src_fragments; *p; p++) {
    src_len += strlen(*p);
  }

  char *src = malloc(src_len + 1);
  size_t n = 0;
  for (p = src_fragments; *p; p++) {
    strcpy(src + n, *p);
    n += strlen(*p);
  }

  return src;
}

static const char *cuda_nvrtc_get_arch(CUdevice dev)
{
  struct {
    int major;
    int minor;
    const char *arch_str;
  } static const x[] = {
    { 3, 0, "compute_30" },
    { 3, 2, "compute_32" },
    { 3, 5, "compute_35" },
    { 3, 7, "compute_37" },
    { 5, 0, "compute_50" },
    { 5, 2, "compute_52" },
    { 5, 3, "compute_53" },
    { 6, 0, "compute_60" },
    { 6, 1, "compute_61" },
    { 6, 2, "compute_62" },
    { 7, 0, "compute_70" },
    { 7, 2, "compute_72" },
    { 7, 5, "compute_75" }
  };

  int major = device_query(dev, COMPUTE_CAPABILITY_MAJOR);
  int minor = device_query(dev, COMPUTE_CAPABILITY_MINOR);

  int chosen = -1;
  for (int i = 0; i < sizeof(x)/sizeof(x[0]); i++) {
    if (x[i].major < major || (x[i].major == major && x[i].minor <= minor)) {
      chosen = i;
    } else {
      break;
    }
  }

  if (chosen == -1) {
    panic(-1, "Unsupported compute capability %d.%d\n", major, minor);
  }
  return x[chosen].arch_str;
}

static char *cuda_nvrtc_build(struct cuda_context *ctx, const char *src)
{
  nvrtcProgram prog;
  NVRTC_SUCCEED(nvrtcCreateProgram(&prog, src, "futhark-cuda", 0, NULL, NULL));

  size_t n_opts, i = 0, i_dyn, n_opts_alloc = 20 + ctx->cfg.num_sizes;
  const char **opts = malloc(n_opts_alloc * sizeof(const char *));
  opts[i++] = "-arch";
  opts[i++] = cuda_nvrtc_get_arch(ctx->nodes[0].dev);
  opts[i++] = "-default-device";
  if (ctx->cfg.debugging) {
    opts[i++] = "-G";
    opts[i++] = "-lineinfo";
  } else {
    opts[i++] = "--disable-warnings";
  }
  i_dyn = i;
  for (size_t j = 0; j < ctx->cfg.num_sizes; j++) {
    opts[i++] = msgprintf("-D%s=%zu", ctx->cfg.size_vars[j],
        ctx->cfg.size_values[j]);
  }
  opts[i++] = msgprintf("-DLOCKSTEP_WIDTH=%zu", ctx->lockstep_width);
  opts[i++] = msgprintf("-DMAX_THREADS_PER_BLOCK=%zu", ctx->max_block_size);
  n_opts = i;

  if (ctx->cfg.debugging) {
    fprintf(stderr, "NVRTC compile options:\n");
    for (size_t j = 0; j < n_opts; j++) {
      fprintf(stderr, "\t%s\n", opts[j]);
    }
    fprintf(stderr, "\n");
  }

  nvrtcResult res = nvrtcCompileProgram(prog, n_opts, opts);
  if (res != NVRTC_SUCCESS) {
    size_t log_size;
    if (nvrtcGetProgramLogSize(prog, &log_size) == NVRTC_SUCCESS) {
      char *log = malloc(log_size);
      if (nvrtcGetProgramLog(prog, log) == NVRTC_SUCCESS) {
        fprintf(stderr,"Compilation log:\n%s\n", log);
      }
      free(log);
    }
    NVRTC_SUCCEED(res);
  }

  for (i = i_dyn; i < n_opts; i++) { free((char *)opts[i]); }
  free(opts);

  char *ptx;
  size_t ptx_size;
  NVRTC_SUCCEED(nvrtcGetPTXSize(prog, &ptx_size));
  ptx = malloc(ptx_size);
  NVRTC_SUCCEED(nvrtcGetPTX(prog, ptx));

  NVRTC_SUCCEED(nvrtcDestroyProgram(&prog));

  return ptx;
}

static void cuda_size_setup(struct cuda_context *ctx)
{
  if (ctx->cfg.default_block_size > ctx->max_block_size) {
    if (ctx->cfg.default_block_size_changed) {
      fprintf(stderr,
          "Note: Device limits default block size to %zu (down from %zu).\n",
          ctx->max_block_size, ctx->cfg.default_block_size);
    }
    ctx->cfg.default_block_size = ctx->max_block_size;
  }
  if (ctx->cfg.default_grid_size > ctx->max_grid_size) {
    if (ctx->cfg.default_grid_size_changed) {
      fprintf(stderr,
          "Note: Device limits default grid size to %zu (down from %zu).\n",
          ctx->max_grid_size, ctx->cfg.default_grid_size);
    }
    ctx->cfg.default_grid_size = ctx->max_grid_size;
  }
  if (ctx->cfg.default_tile_size > ctx->max_tile_size) {
    if (ctx->cfg.default_tile_size_changed) {
      fprintf(stderr,
          "Note: Device limits default tile size to %zu (down from %zu).\n",
          ctx->max_tile_size, ctx->cfg.default_tile_size);
    }
    ctx->cfg.default_tile_size = ctx->max_tile_size;
  }

  for (int i = 0; i < ctx->cfg.num_sizes; i++) {
    const char *size_class, *size_name;
    size_t *size_value, max_value, default_value;

    size_class = ctx->cfg.size_classes[i];
    size_value = &ctx->cfg.size_values[i];
    size_name = ctx->cfg.size_names[i];

    if (strstr(size_class, "group_size") == size_class) {
      max_value = ctx->max_block_size;
      default_value = ctx->cfg.default_block_size;
    } else if (strstr(size_class, "num_groups") == size_class) {
      max_value = ctx->max_grid_size;
      default_value = ctx->cfg.default_grid_size;
    } else if (strstr(size_class, "tile_size") == size_class) {
      max_value = ctx->max_tile_size;
      default_value = ctx->cfg.default_tile_size;
    } else if (strstr(size_class, "threshold") == size_class) {
      max_value = ctx->max_threshold;
      default_value = ctx->cfg.default_threshold;
    } else if (strstr(size_class, "num_nodes") == size_class) {
      max_value = ctx->max_num_nodes;
      default_value = ctx->cfg.default_num_nodes;
    } else {
      panic(1, "Unknown size class for size '%s': %s\n", size_name, size_class);
    }

    if (*size_value == 0) {
      *size_value = default_value;
    } else if (max_value > 0 && *size_value > max_value) {
      fprintf(stderr, "Note: Device limits %s to %zu (down from %zu)\n",
              size_name, max_value, *size_value);
      *size_value = max_value;
    }
  }
}

static void dump_string_to_file(const char *file, const char *buf)
{
  FILE *f = fopen(file, "w");
  assert(f != NULL);
  assert(fputs(buf, f) != EOF);
  assert(fclose(f) == 0);
}

static void load_string_from_file(const char *file, char **obuf, size_t *olen)
{
  char *buf;
  size_t len;
  FILE *f = fopen(file, "r");

  assert(f != NULL);
  assert(fseek(f, 0, SEEK_END) == 0);
  len = ftell(f);
  assert(fseek(f, 0, SEEK_SET) == 0);

  buf = malloc(len + 1);
  assert(fread(buf, 1, len, f) == len);
  buf[len] = 0;
  *obuf = buf;
  if (olen != NULL) {
    *olen = len;
  }

  assert(fclose(f) == 0);
}

static void cuda_module_setup(struct cuda_context *ctx,
    const char *src_fragments[])
{
  char *ptx = NULL, *src = NULL;

  if (ctx->cfg.load_ptx_from == NULL && ctx->cfg.load_program_from == NULL) {
    src = concat_fragments(src_fragments);
    ptx = cuda_nvrtc_build(ctx, src);
  } else if (ctx->cfg.load_ptx_from == NULL) {
    load_string_from_file(ctx->cfg.load_program_from, &src, NULL);
    ptx = cuda_nvrtc_build(ctx, src);
  } else {
    if (ctx->cfg.load_program_from != NULL) {
      fprintf(stderr,
              "WARNING: Loading PTX from %s instead of C code from %s\n",
              ctx->cfg.load_ptx_from, ctx->cfg.load_program_from);
    }

    load_string_from_file(ctx->cfg.load_ptx_from, &ptx, NULL);
  }

  if (ctx->cfg.dump_program_to != NULL) {
    if (src == NULL) {
      src = concat_fragments(src_fragments);
    }
    dump_string_to_file(ctx->cfg.dump_program_to, src);
  }
  if (ctx->cfg.dump_ptx_to != NULL) {
    dump_string_to_file(ctx->cfg.dump_ptx_to, ptx);
  }

  CUDA_SUCCEED(cuModuleLoadData(&ctx->module, ptx));

  free(ptx);
  if (src != NULL) {
    free(src);
  }
}

void cuda_setup(struct cuda_context *ctx, const char *src_fragments[])
{
  CUDA_SUCCEED(cuInit(0));

  cuda_devices_setup(ctx);

  for (int i = 0; i < ctx->max_num_nodes; ++i) {
    CUDA_SUCCEED(cuCtxCreate(&ctx->nodes[i].cu_ctx, 0, ctx->nodes[i].dev));
    free_list_init(&ctx->nodes[i].free_list);
  }

  // All devices are identical, so use the properties from the first.
  ctx->max_block_size = device_query(ctx->nodes[0].dev, MAX_THREADS_PER_BLOCK);
  ctx->max_grid_size = device_query(ctx->nodes[0].dev, MAX_GRID_DIM_X);
  ctx->max_tile_size = sqrt(ctx->max_block_size);
  ctx->max_threshold = 0;
  ctx->lockstep_width = device_query(ctx->nodes[0].dev, WARP_SIZE);

  cuda_size_setup(ctx);
  cuda_module_setup(ctx, src_fragments);
}

CUresult cuda_free_all(struct cuda_context *ctx);

void cuda_cleanup(struct cuda_context *ctx)
{
  CUDA_SUCCEED(cuda_free_all(ctx));
  CUDA_SUCCEED(cuModuleUnload(ctx->module));
  for (int i = 0; i < ctx->max_num_nodes; ++i)
    CUDA_SUCCEED(cuCtxDestroy(ctx->nodes[i].cu_ctx));
}

CUresult cuda_alloc(struct cuda_context *ctx, int node, size_t min_size,
    const char *tag, CUdeviceptr *mem_out)
{
  if (min_size < sizeof(int)) {
    min_size = sizeof(int);
  }

  size_t size;
  if (free_list_find(&ctx->nodes[node].free_list, tag, &size, mem_out) == 0) {
    if (size >= min_size) {
      return CUDA_SUCCESS;
    } else {
      CUresult res = cuMemFree(*mem_out);
      if (res != CUDA_SUCCESS) {
        return res;
      }
    }
  }

  CUresult res = cuMemAlloc(mem_out, min_size);
  while (res == CUDA_ERROR_OUT_OF_MEMORY) {
    CUdeviceptr mem;
    if (free_list_first(&ctx->nodes[node].free_list, &mem) == 0) {
      res = cuMemFree(mem);
      if (res != CUDA_SUCCESS) {
        return res;
      }
    } else {
      break;
    }
    res = cuMemAlloc(mem_out, min_size);
  }

  return res;
}

CUresult cuda_free(struct cuda_context *ctx, int node, CUdeviceptr mem,
    const char *tag)
{
  size_t size;
  CUdeviceptr existing_mem;

  // If there is already a block with this tag, then remove it.
  if (free_list_find(&ctx->nodes[node].free_list, tag, &size, &existing_mem) == 0) {
    CUresult res = cuMemFree(existing_mem);
    if (res != CUDA_SUCCESS) {
      return res;
    }
  }

  CUresult res = cuMemGetAddressRange(NULL, &size, mem);
  if (res == CUDA_SUCCESS) {
    free_list_insert(&ctx->nodes[node].free_list, size, mem, tag);
  }

  return res;
}

CUresult cuda_free_all(struct cuda_context *ctx) {
  CUdeviceptr mem;
  for (int i = 0; i < ctx->max_num_nodes; ++i) {
    free_list_pack(&ctx->nodes[i].free_list);
    while (free_list_first(&ctx->nodes[i].free_list, &mem) == 0) {
      CUresult res = cuMemFree(mem);
      if (res != CUDA_SUCCESS) {
        return res;
      }
    }
  }

  return CUDA_SUCCESS;
}

