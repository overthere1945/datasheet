#pragma once
/**
 * @file sns_ct7117x_ver.h
 *
 * Driver version
 *
 * Copyright (c) 2015-2025 SENSYLINK Microelectronics
 * All Rights Reserved.
 * Confidential and Proprietary - SENSYLINK Microelectronics
 *
 * Copyright (c) 2017-2025 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

/**
 * EDIT HISTORY FOR FILE
 *
 * This section contains comments describing changes made to the module.
 * Notice that changes are listed in reverse chronological order.
 *
 *
 * when         version    who              what
 * --------     --------   ----------       ---------------------------------
 * 02/25/25     010002     Sensylink        Initial Version,Improved the ODR list in properties
 * 03/11/25                Qualcomm	        Fixed crash and read-write issues
 * 04/17/25     010003     Qualcomm	        Add lite driver support
 *                                           1. CT7117X_ATTRIBUTE_DISABLED disables Attribute
 *                                           Publish except for type & availability
 *                                           2. CT7117X_DISABLE_REGISTRY disables registry &
 *                                           allows hardcoded configs (defined in chipsets
 *                                           specific sns_interface_defs.h) to be used.
 *                                           3. CT7117X_POWERRAIL_DISABLED disables power rail
 *                                           voting.
 * 04/29/25     010004     Qualcomm          Add dual sensor support
 * 07/02/25     010005     Qualcomm          Fix Registry requests & event for dual sensors
 *
 **/

// major:01 minor:00 revision:04
#define CT7117X_DRV_VER_MAJOR    0x010005

