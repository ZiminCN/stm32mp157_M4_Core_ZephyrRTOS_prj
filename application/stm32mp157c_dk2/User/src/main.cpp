// Copyright (c) Direct Drive Technology Co., Ltd. All rights reserved.
// Author: Zi Min <jianming.zeng@directdrivetech.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
// #include <zephyr/smf.h>

// #include "fsm.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("STM32MP157 Cotex M4 Start!");

	// std::shared_ptr<FSM> fsm_driver_api = FSM::getInstance();

	// fsm_driver_api->vulcan_fsm_init(VULCAN_INIT_STATE);

	while (true) {
		// LOG_INF("IDLE...");
		k_sleep(K_SECONDS(1));
	}
	return 0;
}