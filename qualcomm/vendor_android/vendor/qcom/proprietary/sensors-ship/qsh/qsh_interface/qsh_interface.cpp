/*
 * Copyright (c) 2021, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include "qsh_interface.h"
#include "qsh_qmi.h"
#include "qsh_glink.h"

qsh_interface* qsh_interface::create(qsh_connection_type connection_type, qsh_conn_config config) {
  switch (connection_type) {
  case QSH_QMI:
    return new qsh_qmi();
  case QSH_GLINK:
    return new qsh_glink(config.glink_config);
  default:
    return nullptr;
  }
}

qsh_interface::~qsh_interface(){

}
