/* Copyright 2020 Espressif Systems (Shanghai) PTE LTD
 *   
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "esp_loader.h"

#include "common.h"

#include "port.h"

#define TARGET_RST_Pin 2
#define TARGET_IO0_Pin 3

#define DEFAULT_BAUD_RATE 115200
#define HIGHER_BAUD_RATE 460800
#define SERIAL_DEVICE "/dev/ttyS0"

static struct {
    char* f_name;
    char* data;
    size_t size;
    uint32_t addr;
} parts[8];

static uint32_t n_parts = 0;

static void help(void) {
    printf("flasher [-p serial_port] [-b baud] [-r RST_gpio] [-0 IO0_gpio ] addr file ...\n"
           "\n"
           "Options:\n"
           "-p,--port   serial port (default %s)\n"
           "-b,--baud   serial port baud rate (default %d baud)\n"
           "-r,--reset  reset GPIO number (default GPIO %d)\n"
           "-0,--io0    IO0 (PROG) gpio number (default GPIO %d)\n"
           "Parameters:\n"
           "addr file   programming address and binary file name\n"
           "            can repeat up to 8 times\n",
           SERIAL_DEVICE, DEFAULT_BAUD_RATE, TARGET_RST_Pin, TARGET_IO0_Pin);
}

int main(int argc, char** argv) {
    loader_raspberry_config_t config = {.device = SERIAL_DEVICE,
                                        .baudrate = DEFAULT_BAUD_RATE,
                                        .reset_trigger_pin = TARGET_RST_Pin,
                                        .io0_trigger_pin = TARGET_IO0_Pin};
    int c;
    opterr = 0;
    for (;;) {
        int option_index = 0;
        static struct option long_options[] = {
            {"help", no_argument, 0, 'h'},       {"port", required_argument, 0, 'p'},
            {"baud", required_argument, 0, 'b'}, {"reset", required_argument, 0, 'r'},
            {"io0", required_argument, 0, '0'},  {0, 0, 0, 0}};
        c = getopt_long(argc, argv, "hp:b:r:0:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'h':
            help();
            return 0;
        case 'p':
            config.device = optarg;
            break;
        case 'b':
            config.baudrate = atoi(optarg);
            break;
        case 'e':
            config.reset_trigger_pin = atoi(optarg);
            break;
        case '0':
            config.io0_trigger_pin = atoi(optarg);
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return -1;
        default:
            abort();
        }
    }

    printf("ESP flasher - port = %s, baud = %d, en_gpio = %d, io0_gpio = %d\n", config.device,
           config.baudrate, config.reset_trigger_pin, config.io0_trigger_pin);

    int i;
    for (i = optind; i < argc; i++) {
        if (*argv[i] == '0' && (*(argv[i] + 1) == 'x' || *(argv[i] + 1) == 'X'))
            parts[n_parts].addr = strtol(argv[i] + 2, NULL, 16);
        else
            parts[n_parts].addr = strtol(argv[i], NULL, 10);
        i++;
        if (i < argc) {
            parts[n_parts].f_name = argv[i];
            FILE* image = fopen(parts[n_parts].f_name, "r");
            if (image == NULL) {
                printf("Error:Failed to open file %s\n", argv[i]);
                return -1;
            }
            fseek(image, 0L, SEEK_END);
            parts[n_parts].size = ftell(image);
            rewind(image);
            parts[n_parts].data = (char*)malloc(parts[n_parts].size);
            if (parts[n_parts].data == NULL) {
                printf("Error: Failed allocate memory\n");
                return -1;
            }
            // copy file content to buffer
            size_t bytes_read = fread(parts[n_parts].data, 1, parts[n_parts].size, image);
            if (bytes_read != parts[n_parts].size) {
                printf("Error occurred while reading file");
                return -1;
            }
            fclose(image);
            n_parts++;
        } else {
            printf("Missing file name\n");
            return -1;
        }
    }
    if (loader_port_raspberry_init(&config) == ESP_LOADER_SUCCESS) {
        if (connect_to_target(HIGHER_BAUD_RATE) == ESP_LOADER_SUCCESS) {
            for (i = 0; i < n_parts; i++) {
                printf("%08x %8lu %s\n", parts[i].addr, parts[i].size, parts[i].f_name);
                flash_binary((uint8_t*)parts[i].data, parts[i].size, parts[i].addr);
            }
            loader_port_reset_target();
        }
    }
    printf("Done\n");
}
