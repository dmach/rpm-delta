IMPORTANT: This is a proof of concept.


rpm-delta
=========
My work on this project started when I was looking at deltarpm code,
because I wanted to understand how difficult would be to extend it
with zstd support. While staring at a piece of code handling CPIO archives,
I realized that it's unnecesarily complex and reinvents the wheel.

It should be simple, ideally a thin wrapper on top of RPM library.
So I started thinking how to achieve that.


Support tools
-------------

I asked my colleague Jarek (jrohel) if he couldn't write 2 simple tools
that would help me with prototyping:

* rpm_split
    Split an RPM into header and uncompressed payload.
* rpm_join
    Create an RPM from header and uncompressed payload.
    The payload is compressed according to the compression
    algorithm specified in the header.

He made it by end of the day.
That allowed me to extract the payload, try creating and applying deltas
and injecting modified payload back to the package.


Binary delta
------------
First of all, I checked which existing tools are out there.
bsdiff has insanely big memory requirements, which makes it unusable on many systems.
xdelta looked more promising and I decided to use it, at least for the fist prototype.


Adoption to RPM
---------------
How the delta works...

Producer extracts CPIO archives from old and new RPMs and creates a delta.

Consumer downloads header of the new RPM and delta.
Then the delta has to be applied on CPIO archive of the old (== installed) RPM.

Here's the list of problems why the CPIO archive cannot be re-created easily:
* Config files on the system were modified or removed
* File order in RPM header and CPIO archive differs
* Files in the new package may have different attributes than on the system

To make the delta work, the payload from the old RPM and payload created on the
system must be identical. I decided to post-process the payload from the old RPM,
hoping that the binary delta tool will deal with shuffling the file order.

This is how it works:
* File order is taken from the RPM header
* Directories are skipped
* %config files are skipped (they frequently change)
* Content of all files is concatenated with no metadata (delta will include all metadata)
* RPM header is not part of delta, it will be range-downloaded from regular repo


How to test
-----------
Download 2 versions of the same package, for example:
* bash-5.0.2-1.fc30.x86_64.rpm
* bash-5.0.7-1.fc30.x86_64.rpm

$ ./rpm-delta.py make bash-5.0.2-1.fc30.x86_64.rpm bash-5.0.7-1.fc30.x86_64.rpm bash_delta

Install bash-5.0.2-1.fc30.x86_64.rpm on the system so it's available for applying delta:
$ dnf install -C bash-5.0.2-1.fc30.x86_64.rpm

The header will be downloaded from repo; here it's created as a by-product of running `./rpm-delta.py make ...`
$ ./rpm-delta.py apply bash_delta bash-5.0.7-1.fc30.x86_64.rpm.hdr bash-5.0.7-1.fc30.x86_64.rpm-reconstructed

$ diff bash-5.0.7-1.fc30.x86_64.rpm bash-5.0.7-1.fc30.x86_64.rpm-reconstructed
$ echo $?


Questions
---------
* Didn't I miss anything important?
* Is this approach equal or better than deltarpm? (speed, size, etc.)
* How would it perform when rewritten into optimized C code?
* Shouldn't we replace xdelta with something else?
