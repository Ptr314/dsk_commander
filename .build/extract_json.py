import json
import sys

def extract_strings(json_file, output_file):
    with open(json_file, "r", encoding="utf-8") as f:
        data = json.load(f)

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("import QtQuick 2.0\nItem {\n")

        def process_dict(parent_key, d):
            for key, value in d.items():
                if isinstance(value, str) and key == "name":
                    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
                    f.write(f'    property string {parent_key}_{key}: qsTr("{escaped}")\n')
                elif isinstance(value, dict):
                    process_dict(key, value)

        process_dict("", data)
        f.write("}\n")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python extract_json.py <input_json> <output_qml>")
        sys.exit(1)

    extract_strings(sys.argv[1], sys.argv[2])
