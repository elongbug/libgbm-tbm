/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>

#include "config.h"
#include "backend.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static void *g_gbm_module;

#if HAVE_TBM
extern const struct gbm_backend gbm_tbm_backend;
#endif

struct backend_desc {
   const char *name;
   const struct gbm_backend *builtin;
};

static const struct backend_desc backends[] = {
#if HAVE_TBM
   { "gbm_tbm.so", &gbm_tbm_backend },
#endif
};

static const void *
load_backend(const struct backend_desc *backend)
{
   char path[PATH_MAX];
   const void *init = NULL;
   void *module = NULL;
   const char *name;
   const char *entrypoint = "gbm_backend";

   if (backend == NULL)
      return NULL;

   if (g_gbm_module)
      return NULL;

   name = backend->name;

   if (backend->builtin) {
      init = backend->builtin;
   } else {
      if (name[0] != '/')
         snprintf(path, sizeof path, MODULEDIR "/%s", name);
      else
         snprintf(path, sizeof path, "%s", name);

      module = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
      if (!module) {
         fprintf(stderr,
                 "failed to load module: %s\n", dlerror());
         return NULL;
      }

      init = dlsym(module, entrypoint);
      if (!init) {
         dlclose(module);
         return NULL;
      }
   }
   g_gbm_module = module;

   return init;
}

static const struct backend_desc *
find_backend(const char *name)
{
   const struct backend_desc *backend = NULL;
   int i;

   for (i = 0; i < ARRAY_SIZE(backends); ++i) {
      if (strcmp(backends[i].name, name) == 0) {
         backend = &backends[i];
         break;
      }
   }

   return backend;
}

struct gbm_device *
_gbm_create_device(int fd)
{
   const struct gbm_backend *backend = NULL;
   struct gbm_device *dev = NULL;
   int i;
   const char *b;

   b = getenv("GBM_BACKEND");
   if (b)
      backend = load_backend(find_backend(b));

   if (backend)
      dev = backend->create_device(fd);

   for (i = 0; i < ARRAY_SIZE(backends) && dev == NULL; ++i) {
      backend = load_backend(&backends[i]);
      if (backend == NULL)
         continue;

      dev = backend->create_device(fd);
   }

   return dev;
}

void
_gbm_close_device(void)
{
   if (g_gbm_module) {
      dlclose(g_gbm_module);
      g_gbm_module = NULL;
   }
}

