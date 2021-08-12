/*
 * lzsa.c - command line compression utility for the LZSA format
 *
 * Copyright (C) 2019 Emmanuel Marty
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*
 * Uses the libdivsufsort library Copyright (c) 2003-2008 Yuta Mori
 *
 * Inspired by LZ4 by Yann Collet. https://github.com/lz4/lz4
 * With help, ideas, optimizations and speed measurements by spke <zxintrospec@gmail.com>
 * With ideas from Lizard by Przemyslaw Skibinski and Yann Collet. https://github.com/inikep/lizard
 * Also with ideas from smallz4 by Stephan Brumme. https://create.stephan-brumme.com/smallz4/
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif
#include "lib.h"

#define OPT_VERBOSE        1
#define OPT_FAVOR_RATIO    2
#define OPT_BACKWARD       4
#define OPT_STATS          8

/*---------------------------------------------------------------------------*/

#ifdef _WIN32
LARGE_INTEGER hpc_frequency;
BOOL hpc_available = FALSE;
#endif

static void do_init_time() {
#ifdef _WIN32
   hpc_frequency.QuadPart = 0;
   hpc_available = QueryPerformanceFrequency(&hpc_frequency);
#endif
}

static long long do_get_time() {
   long long nTime;

#ifdef _WIN32
   if (hpc_available) {
      LARGE_INTEGER nCurTime;

      /* Use HPC hardware for best precision */
      QueryPerformanceCounter(&nCurTime);
      nTime = (long long)(nCurTime.QuadPart * 1000000LL / hpc_frequency.QuadPart);
   }
   else {
      struct _timeb tb;
      _ftime(&tb);

      nTime = ((long long)tb.time * 1000LL + (long long)tb.millitm) * 1000LL;
   }
#else
   struct timeval tm;
   gettimeofday(&tm, NULL);

   nTime = (long long)tm.tv_sec * 1000000LL + (long long)tm.tv_usec;
#endif
   return nTime;
}

/*---------------------------------------------------------------------------*/

static void compression_progress(long long nOriginalSize, long long nCompressedSize) {
   if (nOriginalSize >= 1024 * 1024) {
      fprintf(stdout, "\r" PRId64 " => " PRId64 " (%g %%)     \b\b\b\b\b", nOriginalSize, nCompressedSize, (double)(nCompressedSize * 100.0 / nOriginalSize));
      fflush(stdout);
   }
}

static int do_compress(const char *pszInFilename, const char *pszOutFilename, const char *pszDictionaryFilename, const unsigned int nOptions, const int nMinMatchSize) {
   long long nStartTime = 0LL, nEndTime = 0LL;
   long long nOriginalSize = 0LL, nCompressedSize = 0LL;
   int nCommandCount = 0, nSafeDist = 0;
   int nFlags;
   lzsa_status_t nStatus;
   lzsa_stats stats;

   nFlags = 0;
   if (nOptions & OPT_FAVOR_RATIO)
      nFlags |= LZSA_FLAG_FAVOR_RATIO;
   if (nOptions & OPT_BACKWARD)
      nFlags |= LZSA_FLAG_BACKWARD;

   if (nOptions & OPT_VERBOSE) {
      nStartTime = do_get_time();
   }

   nStatus = lzsa_compress_file(pszInFilename, pszOutFilename, pszDictionaryFilename, nFlags, nMinMatchSize, compression_progress, &nOriginalSize, &nCompressedSize, &nCommandCount, &nSafeDist, &stats);

   if ((nOptions & OPT_VERBOSE)) {
      nEndTime = do_get_time();
   }

   switch (nStatus) {
      case LZSA_ERROR_SRC: fprintf(stderr, "error reading '%s'\n", pszInFilename); break;
      case LZSA_ERROR_DST: fprintf(stderr, "error writing '%s'\n", pszOutFilename); break;
      case LZSA_ERROR_DICTIONARY: fprintf(stderr, "error reading dictionary '%s'\n", pszDictionaryFilename); break;
      case LZSA_ERROR_MEMORY: fprintf(stderr, "out of memory\n"); break;
      case LZSA_ERROR_COMPRESSION: fprintf(stderr, "internal compression error\n"); break;
      case LZSA_ERROR_RAW_TOOLARGE: fprintf(stderr, "error: raw blocks can only be used with files <= 64 Kb\n"); break;
      case LZSA_ERROR_RAW_UNCOMPRESSED: fprintf(stderr, "error: incompressible data needs to be <= 64 Kb in raw blocks\n"); break;
      case LZSA_OK: break;
      default: fprintf(stderr, "unknown compression error %d\n", nStatus); break;
   }

   if (nStatus)
      return 100;

   if ((nOptions & OPT_VERBOSE)) {
      double fDelta = ((double)(nEndTime - nStartTime)) / 1000000.0;
      double fSpeed = ((double)nOriginalSize / 1048576.0) / fDelta;
      fprintf(stdout, "\rCompressed '%s' in %g seconds, %.02g Mb/s, %d tokens (%g bytes/token), " PRId64 " into " PRId64 " bytes ==> %g %%\n",
         pszInFilename, fDelta, fSpeed, nCommandCount, (double)nOriginalSize / (double)nCommandCount,
         nOriginalSize, nCompressedSize, (double)(nCompressedSize * 100.0 / nOriginalSize));
      fprintf(stdout, "Safe distance: %d (0x%X)\n", nSafeDist, nSafeDist);
   }

   if (nOptions & OPT_STATS) {
      if (stats.literals_divisor > 0)
         fprintf(stdout, "Literals: min: %d avg: %d max: %d count: %d\n", stats.min_literals, stats.total_literals / stats.literals_divisor, stats.max_literals, stats.literals_divisor);
      else
         fprintf(stdout, "Literals: none\n");
      if (stats.match_divisor > 0) {
         fprintf(stdout, "Offsets: min: %d avg: %d max: %d reps: %d count: %d\n", stats.min_offset, stats.total_offsets / stats.match_divisor, stats.max_offset, stats.num_rep_offsets, stats.match_divisor);
         fprintf(stdout, "Match lens: min: %d avg: %d max: %d count: %d\n", stats.min_match_len, stats.total_match_lens / stats.match_divisor, stats.max_match_len, stats.match_divisor);
      }
      else {
         fprintf(stdout, "Offsets: none\n");
         fprintf(stdout, "Match lens: none\n");
      }
      if (stats.rle1_divisor > 0) {
         fprintf(stdout, "RLE1 lens: min: %d avg: %d max: %d count: %d\n", stats.min_rle1_len, stats.total_rle1_lens / stats.rle1_divisor, stats.max_rle1_len, stats.rle1_divisor);
      }
      else {
         fprintf(stdout, "RLE1 lens: none\n");
      }
      if (stats.rle2_divisor > 0) {
         fprintf(stdout, "RLE2 lens: min: %d avg: %d max: %d count: %d\n", stats.min_rle2_len, stats.total_rle2_lens / stats.rle2_divisor, stats.max_rle2_len, stats.rle2_divisor);
      }
      else {
         fprintf(stdout, "RLE2 lens: none\n");
      }
   }
   return 0;
}

/*---------------------------------------------------------------------------*/

int main(int argc, char **argv) {
   int i;
   const char *pszInFilename = NULL;
   const char *pszOutFilename = NULL;
   const char *pszDictionaryFilename = NULL;
   int nArgsError = 0;
   int nMinMatchDefined = 0;
   int nMinMatchSize = 0;
   unsigned int nOptions = OPT_FAVOR_RATIO;

   for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "-D")) {
         if (!pszDictionaryFilename && (i + 1) < argc) {
            pszDictionaryFilename = argv[i + 1];
            i++;
         }
         else
            nArgsError = 1;
      }
      else if (!strncmp(argv[i], "-D", 2)) {
         if (!pszDictionaryFilename) {
            pszDictionaryFilename = argv[i] + 2;
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "-m")) {
         if (!nMinMatchDefined && (i + 1) < argc) {
            char *pEnd = NULL;
            nMinMatchSize = (int)strtol(argv[i + 1], &pEnd, 10);
            if (pEnd && pEnd != argv[i + 1] && (nMinMatchSize >= 2 && nMinMatchSize <= 5)) {
               i++;
               nMinMatchDefined = 1;
               nOptions &= (~OPT_FAVOR_RATIO);
            }
            else {
               nArgsError = 1;
            }
         }
         else
            nArgsError = 1;
      }
      else if (!strncmp(argv[i], "-m", 2)) {
         if (!nMinMatchDefined) {
            char *pEnd = NULL;
            nMinMatchSize = (int)strtol(argv[i] + 2, &pEnd, 10);
            if (pEnd && pEnd != (argv[i]+2) && (nMinMatchSize >= 2 && nMinMatchSize <= 5)) {
               nMinMatchDefined = 1;
               nOptions &= (~OPT_FAVOR_RATIO);
            }
            else {
               nArgsError = 1;
            }
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "--prefer-ratio")) {
         if (!nMinMatchDefined) {
            nMinMatchSize = 0;
            nMinMatchDefined = 1;
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "--prefer-speed")) {
         if (!nMinMatchDefined) {
            nMinMatchSize = 3;
            nOptions &= (~OPT_FAVOR_RATIO);
            nMinMatchDefined = 1;
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "-v")) {
         if ((nOptions & OPT_VERBOSE) == 0) {
            nOptions |= OPT_VERBOSE;
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "-b")) {
         if ((nOptions & OPT_BACKWARD) == 0) {
            nOptions |= OPT_BACKWARD;
         }
         else
            nArgsError = 1;
      }
      else if (!strcmp(argv[i], "-stats")) {
      if ((nOptions & OPT_STATS) == 0) {
         nOptions |= OPT_STATS;
      }
      else
         nArgsError = 1;
      }
      else {
         if (!pszInFilename)
            pszInFilename = argv[i];
         else {
            if (!pszOutFilename)
               pszOutFilename = argv[i];
            else
               nArgsError = 1;
         }
      }
   }

   if (nArgsError || !pszInFilename || !pszOutFilename) {
      fprintf(stderr, "lzsa3 command-line tool by Ivanq\n");
      fprintf(stderr, "  (based upon lzsa tool by Emmanuel Marty and spke)\n");
      fprintf(stderr, "usage: %s [-v] <infile> <outfile>\n", argv[0]);
      fprintf(stderr, "   -stats: show compressed data stats\n");
      fprintf(stderr, "       -v: be verbose\n");
      fprintf(stderr, "       -b: compress backward (requires a backward decompressor)\n");
      fprintf(stderr, "       -D <filename>: use dictionary file\n");
      fprintf(stderr, "       -m <value>: minimum match size (3-5) (default: 3)\n");
      fprintf(stderr, "       --prefer-ratio: favor compression ratio (default)\n");
      fprintf(stderr, "       --prefer-speed: favor decompression speed (same as -m3)\n");
      return 100;
   }

   do_init_time();

   return do_compress(pszInFilename, pszOutFilename, pszDictionaryFilename, nOptions, nMinMatchSize);
}
