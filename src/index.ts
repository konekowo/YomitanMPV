import 'core-js/actual/array/fill';
import 'core-js/actual/string/repeat';
import * as config from "./config";

const BASE_PATH = mp.utils.split_path(mp.get_script_file())[0];

mp.msg.info("YomitanMPV loaded!");
config.init(BASE_PATH);

