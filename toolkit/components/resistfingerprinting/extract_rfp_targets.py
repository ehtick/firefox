import re
from collections import defaultdict


def parse_defaults(path):
    pattern = re.compile(r"(DESKTOP_DEFAULT|ANDROID_DEFAULT)\((.+)\)")

    contents = ""
    with open(path) as f:
        contents = f.read()

    defaults = defaultdict(lambda: [])
    for match in pattern.finditer(contents):
        platform = match.group(1)
        target = match.group(2)
        defaults[platform].append(target)

    return defaults


def write_defaults(output, defaults_base, defaults_fpp):
    output.write("export const DefaultTargetsBaseline = {\n")
    for platform in defaults_base:
        output.write(f'\t"{platform}": [')
        for target in defaults_base[platform]:
            output.write(f'"{target}",')
        output.write("],\n")
    output.write("}\n")

    output.write("\n")

    output.write("export const DefaultTargetsFPP = {\n")
    for platform in defaults_fpp:
        output.write(f'\t"{platform}": [')
        for target in defaults_fpp[platform]:
            output.write(f'"{target}",')
        output.write("],\n")
    output.write("}\n")


def parse_targets(path):
    pattern = re.compile(r"ITEM_VALUE\((.+),[\s]+(.+)\)")

    contents = ""
    with open(path) as f:
        contents = f.read()

    targets = {}
    for match in pattern.finditer(contents):
        target = match.group(1)
        value = match.group(2)
        targets[target] = value

    return targets


def write_targets(output, targets):
    output.write("export const Targets = {\n")
    for target, value in targets.items():
        target_w_padding = f'\t"{target}":'.ljust(45)
        output.write(f"{target_w_padding} {value},\n")
    output.write("}\n")


def main(output, targets_path, defaults_base_path, defaults_fpp_path):
    output.write("// This is a generated file. Please do not edit.\n")
    output.write(
        f"// See extract_rfp_targets.py, {targets_path}, {defaults_base_path}, {defaults_fpp_path} files instead.\n"
    )

    write_targets(output, parse_targets(targets_path))
    output.write("\n")
    write_defaults(
        output, parse_defaults(defaults_base_path), parse_defaults(defaults_fpp_path)
    )
