/**
 * tel-plugin-samsung
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact: Ja-young Gu <jygu@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of SAMSUNG ELECTRONICS ("Confidential Information").
 * You shall not disclose such Confidential Information and shall
 * use it only in accordance with the terms of the license agreement you entered into with SAMSUNG ELECTRONICS.
 * SAMSUNG make no representations or warranties about the suitability
 * of the software, either express or implied, including but not
 * limited to the implied warranties of merchantability, fitness for a particular purpose, or non-infringement.
 * SAMSUNG shall not be liable for any damages suffered by licensee as
 * a result of using, modifying or distributing this software or its derivatives.
 */

#ifndef __S_DISPATCH_H__
#define __S_DISPATCH_H__

void do_factory(TcoreHal *h, const ipc_message_type *ipc);
void do_notification_message(TcorePlugin *p, const ipc_message_type *ipc);
void do_notification_sys_message(TcorePlugin *p, const void *data);

#endif