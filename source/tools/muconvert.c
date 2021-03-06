/*
 * muconvert -- command line tool for converting documents
 */

#include "mupdf/fitz.h"

/* input options */
static const char *password = "";
static int alphabits = 8;
static float layout_w = 450;
static float layout_h = 600;
static float layout_em = 12;
static char *layout_css = NULL;

/* output options */
static const char *output = "out.pdf";
static const char *format = NULL;
static const char *options = "";

static fz_context *ctx;
static fz_document *doc;
static fz_document_writer *out;

static void usage(void)
{
	fprintf(stderr,
		"mutool convert version " FZ_VERSION "\n"
		"Usage: mutool convert [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-A -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\n"
		"\t-o -\toutput file name (%%d for page number)\n"
		"\t-F -\toutput format (default inferred from output file name)\n"
		"\t\tcbz, pdf\n"
		"\t-O -\toutput format options\n"
		"\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
	exit(1);
}

int muconvert_main(int argc, char **argv)
{
	int i, k, n, c;

	while ((c = fz_getopt(argc, argv, "p:A:W:H:S:U:o:F:O:")) != -1)
	{
		switch (c)
		{
		default: usage(); break;

		case 'p': password = fz_optarg; break;
		case 'A': alphabits = atoi(fz_optarg); break;
		case 'W': layout_w = atof(fz_optarg); break;
		case 'H': layout_h = atof(fz_optarg); break;
		case 'S': layout_em = atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;

		case 'o': output = fz_optarg; break;
		case 'F': format = fz_optarg; break;
		case 'O': options = fz_optarg; break;
		}
	}

	if (fz_optind == argc)
		usage();

	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_set_aa_level(ctx, alphabits);

	if (layout_css)
	{
		fz_buffer *buf = fz_read_file(ctx, layout_css);
		fz_write_buffer_byte(ctx, buf, 0);
		fz_set_user_css(ctx, (char*)buf->data);
		fz_drop_buffer(ctx, buf);
	}

	/* Open the output document. */
	fz_try(ctx)
		out = fz_new_document_writer(ctx, output, format, options);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot open document: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	for (i = fz_optind; i < argc; ++i)
	{
		doc = fz_open_document(ctx, argv[i]);
		if (fz_needs_password(ctx, doc))
			if (!fz_authenticate_password(ctx, doc, password))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", argv[i]);
		fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);

		n = fz_count_pages(ctx, doc);
		for (k = 0; k < n; ++k)
		{
			fz_matrix ctm;
			fz_rect mediabox;
			fz_page *page;
			fz_device *dev;

			page = fz_load_page(ctx, doc, k);
			fz_bound_page(ctx, page, &mediabox);
			dev = fz_begin_page(ctx, out, &mediabox, &ctm);
			fz_run_page(ctx, page, dev, &ctm, NULL);
			fz_end_page(ctx, out, dev);
			fz_drop_page(ctx, page);
		}

		fz_drop_document(ctx, doc);
	}

	fz_drop_document_writer(ctx, out);
	fz_drop_context(ctx);
	return EXIT_SUCCESS;
}
