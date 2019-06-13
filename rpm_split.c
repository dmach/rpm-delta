#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmts.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int ret = EXIT_SUCCESS;
    FD_t fdi = NULL, gzdi=NULL, fdoHeader = NULL, fdoPayload = NULL;
    void * headerBuf = NULL;
    char * rpmio_flags = NULL;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s in_rpm_file out_header out_payload\n", argv[0]);
        return EXIT_FAILURE;
    }

    do {
        Header h;
        int rc;

        fdi = Fopen(argv[1], "r.ufdio");
        if (Ferror(fdi)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fdi));
            ret = EXIT_FAILURE;
            break;
        }

        fdoHeader = Fopen(argv[2], "w.fdio");
        if (Ferror(fdoHeader)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fdoHeader));
            ret = EXIT_FAILURE;
            break;
        }

        fdoPayload = Fopen(argv[3], "w.fdio");
        if (Ferror(fdoPayload)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fdoPayload));
            ret = EXIT_FAILURE;
            break;
        }

        rpmts ts = rpmtsCreate();
        rpmVSFlags vsflags = RPMVSF_MASK_NODIGESTS | RPMVSF_MASK_NOSIGNATURES | RPMVSF_NOHDRCHK;
        (void) rpmtsSetVSFlags(ts, vsflags);
        rc = rpmReadPackageFile(ts, fdi, "rpm2cpio", &h);
        ts = rpmtsFree(ts);

        switch (rc) {
        case RPMRC_OK:
        case RPMRC_NOKEY:
        case RPMRC_NOTTRUSTED:
            break;
        case RPMRC_NOTFOUND:
            fprintf(stderr, "argument is not an RPM package\n");
            ret = EXIT_FAILURE;
            break;
        case RPMRC_FAIL:
        default:
            fprintf(stderr, "error reading header from package\n");
            ret = EXIT_FAILURE;
            break;
        }

        if (ret != EXIT_SUCCESS)
            break;

        /* Retrieve payload size and compression type from header. */
        uint64_t payloadSize = headerGetNumber(h, RPMTAG_LONGARCHIVESIZE);
        const char *compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);

        /* Retrieve payload offset. */
        off_t payloadOffset = Ftell(fdi);

        /* copy header into fdoHeader */
        if (Fseek(fdi, 0, SEEK_SET) == -1) {
            fprintf(stderr, "cannot seek in input file: %s\n", Fstrerror(fdi));
            ret = EXIT_FAILURE;
            break;
        }
        headerBuf = malloc(payloadOffset);
        Fread(headerBuf, 1, payloadOffset, fdi);
        Fwrite(headerBuf, 1, payloadOffset, fdoHeader);

        /* open descriptor with transparent decompresion */
        rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
        gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
        if (!gzdi) {
            fprintf(stderr, "cannot re-open payload: %s\n", Fstrerror(gzdi));
            ret = EXIT_FAILURE;
            break;
        }
        /* copy (decompress) payload to fdoPayload */
        ret = (ufdCopy(gzdi, fdoPayload) == payloadSize) ? EXIT_SUCCESS : EXIT_FAILURE;

    } while (0);

    if (rpmio_flags)
        free(rpmio_flags);
    if (headerBuf)
        free(headerBuf);
    if (fdoPayload)
        Fclose(fdoPayload);
    if (fdoHeader)
        Fclose(fdoHeader);
    if (gzdi)
        Fclose(gzdi);	/* XXX gzdi == fdi */

    return ret;
}
