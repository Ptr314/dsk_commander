import json
import re
import sys

# Delimiter used in type "name" values to express a two-level (grouped) label,
# e.g. "Agat::140K". The part before the delimiter is the group label.
DELIM = "::"

# QML identifiers must match [A-Za-z_][A-Za-z0-9_]* — replace anything else with '_'.
def sanitize_id(s):
    s = re.sub(r'[^A-Za-z0-9_]', '_', s)
    if s and s[0].isdigit():
        s = '_' + s
    return s

def extract_strings(json_file, output_file):
    with open(json_file, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Group label prefixes (the part before "::"), in first-seen order, deduped.
    group_prefixes = []
    seen_prefixes = set()

    with open(output_file, "w", encoding="utf-8") as f:
        f.write("import QtQuick 2.0\nItem {\n")

        def emit(prop, value):
            escaped = value.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'    property string {prop}: qsTr("{escaped}")\n')

        def process_dict(parent_key, d):
            for key, value in d.items():
                if isinstance(value, str) and key == "name":
                    emit(f'{sanitize_id(parent_key)}_{key}', value)
                    # Also expose the part before "::" as its own translatable
                    # string so the two-level type selector can translate the
                    # group label independently of the full name.
                    pos = value.find(DELIM)
                    if pos > 0:
                        prefix = value[:pos]
                        if prefix not in seen_prefixes:
                            seen_prefixes.add(prefix)
                            group_prefixes.append(prefix)
                elif isinstance(value, dict):
                    process_dict(key, value)

        process_dict("", data)

        # Emit deduplicated group labels, e.g. "Agat", "Irisha", "PC".
        for prefix in group_prefixes:
            emit(f'group_{sanitize_id(prefix)}', prefix)

        f.write("}\n")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python extract_json.py <input_json> <output_qml>")
        sys.exit(1)

    extract_strings(sys.argv[1], sys.argv[2])