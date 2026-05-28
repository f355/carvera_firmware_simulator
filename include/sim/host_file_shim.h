/*
 * This file is part of the Carvera Firmware Simulator.
 *
 * Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SIMULATOR_SIM_HOST_FILE_SHIM_H
#define SIMULATOR_SIM_HOST_FILE_SHIM_H

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef __cplusplus
#include <filesystem>
#endif

#ifdef __cplusplus
extern "C" {
#endif

FILE* sim_fopen(const char* path, const char* mode);
FILE* sim_freopen(const char* path, const char* mode, FILE* stream);
int sim_remove(const char* path);
int sim_rename(const char* old_path, const char* new_path);
DIR* sim_opendir(const char* path);
int sim_mkdir(const char* path, mode_t mode);

#ifdef __cplusplus
}
#endif

#ifndef SIM_HOST_FILE_SHIM_IMPLEMENTATION
#define fopen sim_fopen
#define freopen sim_freopen
#define remove(path) sim_remove(path)
#define rename sim_rename
#define opendir sim_opendir
#define mkdir sim_mkdir
#endif

#endif
