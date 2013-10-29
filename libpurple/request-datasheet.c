/**
 * @file request-datasheet.c Request Datasheet API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "request-datasheet.h"

#include "debug.h"
#include "signals.h"

struct _PurpleRequestDatasheet
{
	guint col_count;
	GArray *col_types;
	GArray *col_titles;

	GList *record_list;
	GHashTable *record_li_by_key;
};

struct _PurpleRequestDatasheetRecord
{
	PurpleRequestDatasheet *sheet;
	gpointer key;
	gchar **data; /* at this point, there is only string data possible */
};

static void
purple_request_datasheet_record_free(PurpleRequestDatasheetRecord *rec);

/***** Datasheet API **********************************************************/

PurpleRequestDatasheet *
purple_request_datasheet_new(void)
{
	PurpleRequestDatasheet *sheet;

	sheet = g_new0(PurpleRequestDatasheet, 1);

	sheet->col_types = g_array_new(FALSE, FALSE,
		sizeof(PurpleRequestDatasheetColumnType));
	sheet->col_titles = g_array_new(FALSE, FALSE, sizeof(gchar *));
	/* XXX: use g_array_set_clear_func when we depend on Glib 2.32 */

	sheet->record_li_by_key = g_hash_table_new(g_direct_hash, g_direct_equal);

	purple_signal_register(sheet, "record-changed",
		purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
		G_TYPE_POINTER, /* (PurpleRequestDatasheet *) */
		G_TYPE_POINTER); /* NULL for all */

	purple_signal_register(sheet, "destroy",
		purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
		G_TYPE_POINTER); /* (PurpleRequestDatasheet *) */

	return sheet;
}

void
purple_request_datasheet_free(PurpleRequestDatasheet *sheet)
{
	guint i;

	g_return_if_fail(sheet != NULL);

	purple_signal_emit(sheet, "destroy", sheet);
	purple_signals_unregister_by_instance(sheet);

	for (i = 0; i < sheet->col_titles->len; i++)
		g_free(g_array_index(sheet->col_titles, gchar *, i));

	g_array_free(sheet->col_titles, TRUE);
	g_array_free(sheet->col_types, TRUE);

	g_hash_table_destroy(sheet->record_li_by_key);
	g_list_free_full(sheet->record_list,
		(GDestroyNotify)purple_request_datasheet_record_free);

	g_free(sheet);
}

void
purple_request_datasheet_add_column(PurpleRequestDatasheet *sheet,
	PurpleRequestDatasheetColumnType type, const gchar *title)
{
	gchar *title_clone;

	g_return_if_fail(sheet != NULL);

	if (sheet->record_list != NULL) {
		purple_debug_error("request-datasheet", "Cannot modify model "
			"when there is already some data");
		return;
	}

	title_clone = g_strdup(title);
	sheet->col_count++;
	g_array_append_val(sheet->col_types, type);
	g_array_append_val(sheet->col_titles, title_clone);
}

guint
purple_request_datasheet_get_column_count(PurpleRequestDatasheet *sheet)
{
	g_return_val_if_fail(sheet != NULL, 0);

	return sheet->col_count;
	/*return sheet->col_types->len;*/
}

PurpleRequestDatasheetColumnType
purple_request_datasheet_get_column_type(PurpleRequestDatasheet *sheet,
	guint col_no)
{
	g_return_val_if_fail(sheet != NULL, 0);

	return g_array_index(sheet->col_types,
		PurpleRequestDatasheetColumnType, col_no);
}

const gchar *
purple_request_datasheet_get_column_title(PurpleRequestDatasheet *sheet,
	guint col_no)
{
	g_return_val_if_fail(sheet != NULL, 0);

	return g_array_index(sheet->col_titles, gchar *, col_no);
}

const GList *
purple_request_datasheet_get_records(PurpleRequestDatasheet *sheet)
{
	g_return_val_if_fail(sheet != NULL, NULL);

	return sheet->record_list;
}

/***** Datasheet record API ***************************************************/

static PurpleRequestDatasheetRecord *
purple_request_datasheet_record_new(void)
{
	return g_new0(PurpleRequestDatasheetRecord, 1);
}

static void
purple_request_datasheet_record_free(PurpleRequestDatasheetRecord *rec)
{
	g_strfreev(rec->data);
	g_free(rec);
}

gpointer
purple_request_datasheet_record_get_key(const PurpleRequestDatasheetRecord *rec)
{
	g_return_val_if_fail(rec != NULL, NULL);

	return rec->key;
}

PurpleRequestDatasheet *
purple_request_datasheet_record_get_datasheet(
	PurpleRequestDatasheetRecord *rec)
{
	g_return_val_if_fail(rec != NULL, NULL);

	return rec->sheet;
}

PurpleRequestDatasheetRecord *
purple_request_datasheet_record_find(PurpleRequestDatasheet *sheet,
	gpointer key)
{
	GList *it;

	g_return_val_if_fail(sheet != NULL, NULL);

	it = g_hash_table_lookup(sheet->record_li_by_key, key);
	if (!it)
		return NULL;

	return it->data;
}

PurpleRequestDatasheetRecord *
purple_request_datasheet_record_add(PurpleRequestDatasheet *sheet,
	gpointer key)
{
	PurpleRequestDatasheetRecord *rec;

	g_return_val_if_fail(sheet != NULL, NULL);

	rec = purple_request_datasheet_record_find(sheet, key);
	if (rec != NULL)
		return rec;

	rec = purple_request_datasheet_record_new();
	rec->sheet = sheet;
	rec->key = key;

	/* we don't allow modifying collumn count when datasheet contains
	 * any records */
	rec->data = g_new0(gchar*,
		purple_request_datasheet_get_column_count(sheet) + 1);

	sheet->record_list = g_list_append(sheet->record_list, rec);
	g_hash_table_insert(sheet->record_li_by_key, key,
		g_list_find(sheet->record_list, rec));

	purple_signal_emit(sheet, "record-changed", sheet, key);

	return rec;
}

void
purple_request_datasheet_record_remove(PurpleRequestDatasheet *sheet,
	gpointer key)
{
	GList *it;

	g_return_if_fail(sheet != NULL);

	it = g_hash_table_lookup(sheet->record_li_by_key, key);
	if (it == NULL)
		return;

	purple_request_datasheet_record_free(it->data);
	sheet->record_list = g_list_delete_link(sheet->record_list, it);
	g_hash_table_remove(sheet->record_li_by_key, key);

	purple_signal_emit(sheet, "record-changed", sheet, key);
}

void
purple_request_datasheet_record_remove_all(PurpleRequestDatasheet *sheet)
{
	g_return_if_fail(sheet != NULL);

	g_list_free_full(sheet->record_list,
		(GDestroyNotify)purple_request_datasheet_record_free);
	sheet->record_list = NULL;
	g_hash_table_remove_all(sheet->record_li_by_key);

	purple_signal_emit(sheet, "record-changed", sheet, NULL);
}

static void
purple_request_datasheet_record_set_common_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *data)
{
	g_return_if_fail(rec != NULL);
	g_return_if_fail(
		purple_request_datasheet_get_column_count(rec->sheet) > col_no);

	if (g_strcmp0(rec->data[col_no], data) == 0)
		return;

	/* we assume, model hasn't changed */
	g_free(rec->data[col_no]);
	rec->data[col_no] = g_strdup(data);

	purple_signal_emit(rec->sheet, "record-changed", rec->sheet, rec->key);
}

void
purple_request_datasheet_record_set_string_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *data)
{
	g_return_if_fail(rec != NULL);
	g_return_if_fail(purple_request_datasheet_get_column_type(rec->sheet,
		col_no) == PURPLE_REQUEST_DATASHEET_COLUMN_STRING);

	purple_request_datasheet_record_set_common_data(rec, col_no, data);
}

void
purple_request_datasheet_record_set_image_data(
	PurpleRequestDatasheetRecord *rec, guint col_no, const gchar *stock_id)
{
	g_return_if_fail(rec != NULL);
	g_return_if_fail(purple_request_datasheet_get_column_type(rec->sheet,
		col_no) == PURPLE_REQUEST_DATASHEET_COLUMN_IMAGE);

	purple_request_datasheet_record_set_common_data(rec, col_no, stock_id);
}

const gchar *
purple_request_datasheet_record_get_common_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no)
{
	g_return_val_if_fail(rec != NULL, NULL);
	g_return_val_if_fail(
		purple_request_datasheet_get_column_count(rec->sheet) > col_no,
		NULL);

	return rec->data[col_no];
}

const gchar *
purple_request_datasheet_record_get_string_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no)
{
	g_return_val_if_fail(rec != NULL, NULL);
	g_return_val_if_fail(purple_request_datasheet_get_column_type(
		rec->sheet, col_no) == PURPLE_REQUEST_DATASHEET_COLUMN_STRING,
		NULL);

	return purple_request_datasheet_record_get_common_data(rec, col_no);
}

const gchar *
purple_request_datasheet_record_get_image_data(
	const PurpleRequestDatasheetRecord *rec, guint col_no)
{
	g_return_val_if_fail(rec != NULL, NULL);
	g_return_val_if_fail(purple_request_datasheet_get_column_type(
		rec->sheet, col_no) == PURPLE_REQUEST_DATASHEET_COLUMN_IMAGE,
		NULL);

	return purple_request_datasheet_record_get_common_data(rec, col_no);
}
