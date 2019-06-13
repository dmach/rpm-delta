#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmts.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int ret = EXIT_SUCCESS;
    FD_t fRpm = NULL, fCompress=NULL, fHeader = NULL, fPayload = NULL;
    void * headerBuf = NULL;
    char * rpmioFlags = NULL;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s in_header in_payload out_rpm_file \n", argv[0]);
        return EXIT_FAILURE;
    }

    do {
        Header h;
        int rc;

        fHeader = Fopen(argv[1], "r.fdio");
        if (Ferror(fHeader)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fHeader));
            ret = EXIT_FAILURE;
            break;
        }

        fPayload = Fopen(argv[2], "r.fdio");
        if (Ferror(fPayload)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fPayload));
            ret = EXIT_FAILURE;
            break;
        }

        fRpm = Fopen(argv[3], "w.fdio");
        if (Ferror(fRpm)) {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], Fstrerror(fRpm));
            ret = EXIT_FAILURE;
            break;
        }

        rpmts ts = rpmtsCreate();
        rpmVSFlags vsflags = RPMVSF_MASK_NODIGESTS | RPMVSF_MASK_NOSIGNATURES | RPMVSF_NOHDRCHK;
        (void) rpmtsSetVSFlags(ts, vsflags);
        rc = rpmReadPackageFile(ts, fHeader, "rpm2cpio", &h);
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
        const char * compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
        const char * payloadFlags = headerGetString(h, RPMTAG_PAYLOADFLAGS);

        off_t headerSize = fdSize(fHeader);

        /* copy header into fRpm */
        if (Fseek(fHeader, 0, SEEK_SET) == -1) {
            fprintf(stderr, "cannot seek in input file: %s\n", Fstrerror(fHeader));
            ret = EXIT_FAILURE;
            break;
        }
        headerBuf = malloc(headerSize);
        size_t readed = Fread(headerBuf, 1, headerSize, fHeader);
        size_t writed = Fwrite(headerBuf, 1, headerSize, fRpm);

        /* open descriptor with transparent decompresion */
        rpmioFlags = rstrscat(NULL, "w", payloadFlags, ".", compr ? compr : "gzip", NULL);
        fCompress = Fdopen(fRpm, rpmioFlags);	/* XXX gzdi == fdi */
        if (!fCompress) {
            fprintf(stderr, "cannot re-open payload: %s\n", Fstrerror(fCompress));
            ret = EXIT_FAILURE;
            break;
        }
        /* join (compressed) payload to fRpm */
        ret = (ufdCopy(fPayload, fCompress) == payloadSize) ? EXIT_SUCCESS : EXIT_FAILURE;

    } while (0);

    if (rpmioFlags)
        free(rpmioFlags);
    if (headerBuf)
        free(headerBuf);
    if (fPayload)
        Fclose(fPayload);
    if (fHeader)
        Fclose(fHeader);
    if (fCompress)
        Fclose(fCompress);	/* XXX gzdi == fdi */

    return ret;
}
