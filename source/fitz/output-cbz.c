#include "mupdf/fitz.h"

#include <zlib.h>

typedef struct fz_cbz_writer_s fz_cbz_writer;

struct fz_cbz_writer_s
{
	fz_document_writer super;
	fz_zip_writer *zip;
	int resolution;
	fz_pixmap *pixmap;
	int count;
};

static fz_device *
cbz_begin_page(fz_context *ctx, fz_document_writer *wri_, const fz_rect *mediabox, fz_matrix *ctm)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_rect bbox;
	fz_irect ibbox;

	fz_scale(ctm, wri->resolution / 72, wri->resolution / 72);
	bbox = *mediabox;
	fz_transform_rect(&bbox, ctm);
	fz_round_rect(&ibbox, &bbox);

	wri->pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), &ibbox);
	fz_clear_pixmap_with_value(ctx, wri->pixmap, 0xFF);

	return fz_new_draw_device(ctx, wri->pixmap);
}

static void
cbz_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_buffer *buffer;
	char name[40];

	wri->count += 1;

	fz_snprintf(name, sizeof name, "p%04d.png", wri->count);

	buffer = fz_new_buffer_from_pixmap_as_png(ctx, wri->pixmap);
	fz_try(ctx)
		fz_write_zip_entry(ctx, wri->zip, name, buffer, 0);
	fz_always(ctx)
		fz_drop_buffer(ctx, buffer);
	fz_catch(ctx)
		fz_rethrow(ctx);

	fz_drop_pixmap(ctx, wri->pixmap);
	wri->pixmap = NULL;
}

static void
cbz_drop_imp(fz_context *ctx, fz_document_writer *wri_)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_try(ctx)
		fz_drop_zip_writer(ctx, wri->zip);
	fz_always(ctx)
		fz_drop_pixmap(ctx, wri->pixmap);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

fz_document_writer *
fz_new_cbz_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_cbz_writer *wri = fz_malloc_struct(ctx, fz_cbz_writer);

	wri->super.begin_page = cbz_begin_page;
	wri->super.end_page = cbz_end_page;
	wri->super.drop_imp = cbz_drop_imp;

	fz_try(ctx)
		wri->zip = fz_new_zip_writer(ctx, path);
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	// TODO: getopt-like comma separated list of options
	if (options)
		wri->resolution = atoi(options);

	if (wri->resolution == 0)
		wri->resolution = 96;

	return (fz_document_writer*)wri;
}
