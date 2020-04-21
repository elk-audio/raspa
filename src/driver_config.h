/*
 * Copyright 2018-2020 Modern Ancient Instruments Networked AB, dba Elk
 * RASPA is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * RASPA is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RASPA.
 * If not, see http://www.gnu.org/licenses/ .
 */

 /**
 * @brief Configuration macros for RTDM audio driver.
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_CONFIG_H_H
#define RASPA_CONFIG_H_H

#define RASPA_IOC_MAGIC     'r'

#define RASPA_IRQ_WAIT              _IO(RASPA_IOC_MAGIC, 1)
#define RASPA_PROC_START            _IO(RASPA_IOC_MAGIC, 3)
#define RASPA_USERPROC_FINISHED     _IOW(RASPA_IOC_MAGIC, 4, int)
#define RASPA_PROC_STOP             _IO(RASPA_IOC_MAGIC, 5)

#define RASPA_DEVICE_NAME            "/dev/rtdm/audio_rtdm"
#define RASPA_MODULE_PARAMETERS_PATH "/sys/module/audio_rtdm/parameters"

#define RASPA_PROCESSING_TASK_PRIO  90

 /**
  * @brief Enumeration to denote various codec sample formats
  */
enum RaspaCodecFormat
{
    INT24_LJ = 1,
    INT24_I2S,
    INT24_RJ,
    INT32_RJ,
    NUM_CODEC_FORMATS
};

#endif //RASPA_CONFIG_H_H