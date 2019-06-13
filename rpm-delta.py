#!/usr/bin/python3


import argparse
import os
import subprocess
import sys

# python3-cpioarchive
import cpioarchive

# python3-rpm
import rpm


def run(cmd, **kwargs):
    proc = subprocess.Popen(cmd, **kwargs)
    proc.communicate()


def get_rpm_header(path):
    ts = rpm.TransactionSet("/")
    ts.setVSFlags(rpm._RPMVSF_NOSIGNATURES|rpm._RPMVSF_NODIGESTS)
    with open(path, "rb") as package:
        fdno = package.fileno()
        hdr = ts.hdrFromFdno(fdno)
        return hdr


def get_rpm_header_from_rpmdb(name, arch=None):
    ts = rpm.TransactionSet("/")
    ts.setVSFlags(rpm._RPMVSF_NOSIGNATURES|rpm._RPMVSF_NODIGESTS)
    matches = ts.dbMatch("name", name)
    for hdr in matches:
        if arch and hdr[rpm.RPMTAG_ARCH].decode("utf8") != arch:
            continue
        return hdr
    raise RuntimeError("Package not found in rpmdb: name=%s, arch=%s" % (name, arch))


def rpm_delta_make(in_oldrpm, in_newrpm, out_delta):
    in_oldrpm_blob = in_oldrpm + ".blob"
    create_blob_from_rpm(in_oldrpm, in_oldrpm_blob)

    in_newrpm_hdr = in_newrpm + ".hdr"
    in_newrpm_cpio = in_newrpm + ".cpio"
    run(["./rpm_split", in_newrpm, in_newrpm_hdr, in_newrpm_cpio])

    run(["xdelta", "-f", "-e", "-s", in_oldrpm_blob, in_newrpm_cpio, out_delta])


def rpm_delta_apply(in_oldrpm, in_delta, in_newrpm_hdr, out_newrpm):
    if in_oldrpm:
        in_oldrpm_blob = in_oldrpm + ".blob"
        create_blob_from_rpm(in_oldrpm, in_oldrpm_blob)
    else:
        hdr = get_rpm_header(in_newrpm_hdr)
        name = hdr[rpm.RPMTAG_NAME].decode("utf-8")
        arch = hdr[rpm.RPMTAG_ARCH].decode("utf-8")

        in_oldrpm_blob = "%s-%s.blob" % (name, arch)
        create_blob_from_system(in_oldrpm_blob, name, arch)

    out_newrpm_cpio = out_newrpm + ".cpio"
    run(["xdelta3", "-f", "-d", "-s", in_oldrpm_blob, in_delta, out_newrpm_cpio])
    run(["./rpm_join", in_newrpm_hdr, out_newrpm_cpio, out_newrpm])


def create_blob_from_rpm(in_rpm, out_blob):
    rpm_hdr = in_rpm + ".hdr"
    rpm_cpio = in_rpm + ".cpio"
    run(["./rpm_split", in_rpm, rpm_hdr, rpm_cpio])

    hdr = get_rpm_header(in_rpm)
    blob = open(out_blob, "wb")

    for num, rpm_file in enumerate(hdr[rpm.RPMTAG_FILENAMES]):
        rpm_file = rpm_file.decode("utf8")
        is_config = bool(hdr[rpm.RPMTAG_FILEFLAGS][num] & rpm.RPMFILE_CONFIG)
        is_symlink = bool(hdr[rpm.RPMTAG_FILELINKTOS][num])

        # config files frequently change -> don't include them in delta source
        if is_config:
            continue

        if is_symlink:
            continue

        # this is highly inefficient, just for demonstration purposes
        cpio = cpioarchive.CpioArchive(name=rpm_cpio)
        found = False
        for cpio_fileobj in cpio:
            cpio_path = cpio_fileobj.name

            # fix path, turn it into absolute path
            if cpio_path.startswith("./"):
                cpio_path = cpio_path[1:]

            if rpm_file == cpio_path:
                found = True
                blob.write(cpio_fileobj.read())
                cpio_fileobj.close()
                break
            cpio_fileobj.close()
        cpio.close()

        if not found:
            raise RuntimeError("File not found in the cpio archive: %s" % rpm_file)

    blob.close()


def create_blob_from_system(out_blob, name, arch):
    hdr = get_rpm_header_from_rpmdb(name, arch)
    blob = open(out_blob, "wb")

    for num, rpm_file in enumerate(hdr[rpm.RPMTAG_FILENAMES]):
        rpm_file = rpm_file.decode("utf8")
        is_config = bool(hdr[rpm.RPMTAG_FILEFLAGS][num] & rpm.RPMFILE_CONFIG)
        is_symlink = bool(hdr[rpm.RPMTAG_FILELINKTOS][num])

        # config files frequently change -> don't include them in delta source
        if is_config:
            continue

        if is_symlink:
            continue

        if os.path.isdir(rpm_file):
            continue

        if os.path.islink(rpm_file):
            continue

        blob.write(open(rpm_file, "rb").read())

    blob.close()


def get_parser():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(help="commands", dest="command")

    make = commands.add_parser("make")
    make.add_argument("in_oldrpm")
    make.add_argument("in_newrpm")
    make.add_argument("out_delta")

    apply = commands.add_parser("apply")
    apply.add_argument("-r", dest="in_oldrpm", help="if not specified, an installed RPM is used")
    apply.add_argument("in_delta")
    apply.add_argument("in_newrpm_hdr")
    apply.add_argument("out_newrpm")

    return parser


def main():
    parser = get_parser()
    args = parser.parse_args()

    if args.command == "make":
        rpm_delta_make(args.in_oldrpm, args.in_newrpm, args.out_delta)
    elif args.command == "apply":
        rpm_delta_apply(args.in_oldrpm, args.in_delta, args.in_newrpm_hdr, args.out_newrpm)
    else:
        parser.error("Please specify a command")


if __name__ == "__main__":
    main()
