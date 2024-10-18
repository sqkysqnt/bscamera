#ifndef COLORS_H
#define COLORS_H

// Define structures to hold name and hex pairs
struct GelColor {
    const char* name;
    const char* hex;
};

// Remove PROGMEM and store data directly
const GelColor gels[] = {
    {"kelly green", "#19ffc1"}, {"r92", "#19ffc1"}, {"rosco 92", "#19ffc1"},
    {"pink red", "#FFFFFF"}, {"e0", "#FFFFFF"}, {"ecolor 0", "#FFFFFF"},
    {"med yellow", "#fff200"}, {"r10", "#fff200"}, {"rosco 10", "#fff200"},
    {"spring yellow", "#f2ff30"}, {"e100", "#f2ff30"}, {"rosco 100", "#f2ff30"},
    {"saffron", "#ffeb0f"}, {"e101", "#ffeb0f"}, {"rosco 101", "#ffeb0f"},
    {"medium amber", "#ffe74a"}, {"e102", "#ffe74a"}, {"rosco 102", "#ffe74a"},
    {"warm straw", "#ffe7c4"}, {"e103", "#ffe7c4"}, {"rosco 103", "#ffe7c4"},
    {"deep amber", "#fcd628"}, {"e104", "#fcd628"}, {"rosco 104", "#fcd628"},
    {"orange", "#ff760d"}, {"e105", "#ff760d"}, {"rosco 105", "#ff760d"},
    {"primary red", "#de0000"}, {"e106", "#de0000"}, {"rosco 106", "#de0000"},
    {"nymph pink", "#ff809f"}, {"e107", "#ff809f"}, {"rosco 107", "#ff809f"},
    {"english rose", "#faad96"}, {"e108", "#faad96"}, {"rosco 108", "#faad96"},
    {"lt. salmon", "#ff919c"}, {"e109", "#ff919c"}, {"rosco 109", "#ff919c"},
    {"middle rose", "#ffa3ca"}, {"e110", "#ffa3ca"}, {"rosco 110", "#ffa3ca"},
    {"dark pink", "#ff63a4"}, {"e111", "#ff63a4"}, {"rosco 111", "#ff63a4"},
    {"magenta", "#ff004d"}, {"e113", "#ff004d"}, {"rosco 113", "#ff004d"},
    {"peacock blue", "#00c9bf"}, {"e115", "#00c9bf"}, {"rosco 115", "#00c9bf"},
    {"med blue-green", "#009e96"}, {"e116", "#009e96"}, {"rosco 116", "#009e96"},
    {"steel blue", "#a3e2ff"}, {"e117", "#a3e2ff"}, {"rosco 117", "#a3e2ff"},
    {"light blue", "#00b7ff"}, {"e118", "#00b7ff"}, {"rosco 118", "#00b7ff"},
    {"dark sky blue", "#3300d9"}, {"e119", "#3300d9"}, {"rosco 119", "#3300d9"},
    {"dark blue", "#2800c9"}, {"e120", "#2800c9"}, {"rosco 120", "#2800c9"},
    {"light green yellow", "#93ff54"}, {"e121", "#93ff54"}, {"rosco 121", "#93ff54"},
    {"fern green", "#74f55d"}, {"e122", "#74f55d"}, {"rosco 122", "#74f55d"},
    {"dark green", "#00ab44"}, {"e124", "#00ab44"}, {"rosco 124", "#00ab44"},
    {"orchid", "#d400db"}, {"e126", "#d400db"}, {"rosco 126", "#d400db"},
    {"smokey pink", "#bb334c"}, {"e127", "#bb334c"}, {"rosco 127", "#bb334c"},
    {"bright pink", "#ff177f"}, {"e128", "#ff177f"}, {"rosco 128", "#ff177f"},
    {"clear", "#ffffff"}, {"e130", "#ffffff"}, {"rosco 130", "#ffffff"},
    {"marine blue", "#02e3cc"}, {"e131", "#02e3cc"}, {"rosco 131", "#02e3cc"},
    {"primary blue", "#5286ff"}, {"e132", "#5286ff"}, {"rosco 132", "#5286ff"},
    {"coral", "#f5632f"}, {"e134", "#f5632f"}, {"rosco 134", "#f5632f"},
    {"dip golden amber", "#ff4a00"}, {"e135", "#ff4a00"}, {"rosco 135", "#ff4a00"},
    {"surprise pink", "#e2c7ff"}, {"e136", "#e2c7ff"}, {"rosco 136", "#e2c7ff"},
    {"spec lavender", "#b695fc"}, {"e137", "#b695fc"}, {"rosco 137", "#b695fc"},
    {"pale green", "#b4ffa8"}, {"e138", "#b4ffa8"}, {"rosco 138", "#b4ffa8"},
    {"primary green", "#009107"}, {"e139", "#009107"}, {"rosco 139", "#009107"},
    {"summer blue", "#38caff"}, {"e140", "#38caff"}, {"rosco 140", "#38caff"},
    {"bright blue", "#00acf0"}, {"e141", "#00acf0"}, {"rosco 141", "#00acf0"},
    {"pale violet", "#aa96ff"}, {"e142", "#aa96ff"}, {"rosco 142", "#aa96ff"},
    {"pale navy blue", "#007194"}, {"e143", "#007194"}, {"rosco 143", "#007194"},
    {"no color blue", "#4fc4ff"}, {"e144", "#4fc4ff"}, {"rosco 144", "#4fc4ff"},
    {"apricot", "#ff7438"}, {"e147", "#ff7438"}, {"rosco 147", "#ff7438"},
    {"bright rose", "#ff1472"}, {"e148", "#ff1472"}, {"rosco 148", "#ff1472"},
    {"gold tint", "#ffc0b5"}, {"e151", "#ffc0b5"}, {"rosco 151", "#ffc0b5"},
    {"pale gold", "#ffcaa8"}, {"e152", "#ffcaa8"}, {"rosco 152", "#ffcaa8"},
    {"pale salmon", "#ffb2ba"}, {"e153", "#ffb2ba"}, {"rosco 153", "#ffb2ba"},
    {"french rose", "#ffb2ba"}, {"e154", "#ffb2ba"}, {"rosco 154", "#ffb2ba"},
    {"sepia", "#c57951"}, {"e156", "#c57951"}, {"rosco 156", "#c57951"},
    {"pink", "#ff4551"}, {"e157", "#ff4551"}, {"rosco 157", "#ff4551"},
    {"deep orange", "#ff5e00"}, {"e158", "#ff5e00"}, {"rosco 158", "#ff5e00"},
    {"no color straw", "#fffae0"}, {"e159", "#fffae0"}, {"rosco 159", "#fffae0"},
    {"moody blue", "#4aabff"}, {"e161", "#4aabff"}, {"rosco 161", "#4aabff"},
    {"bastard amber", "#ffcfa8"}, {"e162", "#ffcfa8"}, {"rosco 162", "#ffcfa8"},
    {"flame red", "#f02225"}, {"e164", "#f02225"}, {"rosco 164", "#f02225"},
    {"moon blue", "#1cacff"}, {"e165", "#1cacff"}, {"rosco 165", "#1cacff"},
    {"pale red", "#ff3352"}, {"e166", "#ff3352"}, {"rosco 166", "#ff3352"},
    {"lavender blue", "#daadff"}, {"e170", "#daadff"}, {"rosco 170", "#daadff"},
    {"lagoon blue", "#00aacc"}, {"e172", "#00aacc"}, {"rosco 172", "#00aacc"},
    {"dark steel blue", "#52b4ff"}, {"e174", "#52b4ff"}, {"rosco 174", "#52b4ff"},
    {"chrome orange", "#ff9900"}, {"e179", "#ff9900"}, {"rosco 179", "#ff9900"},
    {"dark lavender", "#8b2bff"}, {"e180", "#8b2bff"}, {"rosco 180", "#8b2bff"},
    {"real congo blue", "#29007a"}, {"e181", "#29007a"}, {"rosco 181", "#29007a"},
    {"moonlight blue", "#00baf2"}, {"e183", "#00baf2"}, {"rosco 183", "#00baf2"},
    {"cosmetic peach", "#000000"}, {"e184", "#000000"}, {"rosco 184", "#000000"},
    {"flesh pink", "#ff639f"}, {"e192", "#ff639f"}, {"rosco 192", "#ff639f"},
    {"rosy amber", "#ff454b"}, {"e193", "#ff454b"}, {"rosco 193", "#ff454b"},
    {"medium lavender", "#ac82ff"}, {"e194", "#ac82ff"}, {"rosco 194", "#ac82ff"},
    {"zenith blue", "#0003cc"}, {"e195", "#0003cc"}, {"rosco 195", "#0003cc"},
    {"true blue", "#00a1ff"}, {"e196", "#00a1ff"}, {"rosco 196", "#00a1ff"},
    {"alice blue", "#1958cf"}, {"e197", "#1958cf"}, {"rosco 197", "#1958cf"},
    {"palace blue", "#43009c"}, {"e198", "#43009c"}, {"rosco 198", "#43009c"},
    {"city blue", "#0f5bff"}, {"e200", "#0f5bff"}, {"rosco 200", "#0f5bff"},
    {"southern sky", "#73a9ff"}, {"e201", "#73a9ff"}, {"rosco 201", "#73a9ff"},
    {"apricot", "#ff9b30"}, {"e204", "#ff9b30"}, {"rosco 204", "#ff9b30"},
    {"spice", "#974400"}, {"e208", "#974400"}, {"rosco 208", "#974400"},
    {"azure blue", "#1ad8d8"}, {"e241", "#1ad8d8"}, {"rosco 241", "#1ad8d8"},
    {"lime green", "#f6ffe0"}, {"e278", "#f6ffe0"}, {"rosco 278", "#f6ffe0"},
    {"antique rose", "#facde0"}, {"e248", "#facde0"}, {"rosco 248", "#facde0"},
    {"sunset red", "#ff7438"}, {"e25", "#ff7438"}, {"rosco 25", "#ff7438"},
    {"fire red", "#ff390b"}, {"e19", "#ff390b"}, {"rosco 19", "#ff390b"},
    {"gold rush", "#ff871c"}, {"e20", "#ff871c"}, {"rosco 20", "#ff871c"},
    {"bastard amber", "#ffcdab"}, {"r2", "#ffcdab"}, {"rosco 2", "#ffcdab"},
    // Add remaining colors following the same format
};


#endif
