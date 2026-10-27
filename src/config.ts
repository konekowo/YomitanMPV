// @ts-ignore
import defaultConfig from "./config.default.jsonc"
import { parse } from 'jsonc-parser';
import merge from 'lodash.merge';

const CONFIG_FILE_NAME = "YomitanMPV-config.jsonc";

export interface Config {
    "yomitan_api_url": string
}

export let CONFIG: Config = parse(defaultConfig);

export function init(basePath: string) {
    const configPath = mp.utils.join_path(basePath, CONFIG_FILE_NAME);

    if (mp.utils.file_info(configPath)) {
        const content = mp.utils.read_file(configPath);

        const parsed = parse(content);

        CONFIG = merge({}, CONFIG, parsed);

        mp.msg.debug("Config Loaded: " + JSON.stringify(CONFIG));
    } else {
        mp.utils.write_file("file://" + configPath, defaultConfig);

        mp.msg.info("YomitanMPV config file created!");
        mp.osd_message("YomitanMPV config file created!", 3);
    }
}