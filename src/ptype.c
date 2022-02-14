#include "vctrs.h"
#include "type-data-frame.h"
#include "decl/ptype-decl.h"


// [[ register() ]]
r_obj* ffi_ptype(r_obj* x, r_obj* x_arg_ffi, r_obj* frame) {
  struct vctrs_arg x_arg = vec_as_arg(x_arg_ffi);
  struct r_lazy call = { .x = r_syms.call, .env = frame };
  return vec_ptype(x, &x_arg, call);
}

r_obj* vec_ptype(r_obj* x, struct vctrs_arg* x_arg, struct r_lazy call) {
  switch (vec_typeof(x)) {
  case vctrs_type_null:        return r_null;
  case vctrs_type_unspecified: return vctrs_shared_empty_uns;
  case vctrs_type_logical:     return vec_ptype_slice(x, vctrs_shared_empty_lgl);
  case vctrs_type_integer:     return vec_ptype_slice(x, vctrs_shared_empty_int);
  case vctrs_type_double:      return vec_ptype_slice(x, vctrs_shared_empty_dbl);
  case vctrs_type_complex:     return vec_ptype_slice(x, vctrs_shared_empty_cpl);
  case vctrs_type_character:   return vec_ptype_slice(x, vctrs_shared_empty_chr);
  case vctrs_type_raw:         return vec_ptype_slice(x, vctrs_shared_empty_raw);
  case vctrs_type_list:        return vec_ptype_slice(x, vctrs_shared_empty_list);
  case vctrs_type_dataframe:   return df_ptype(x, true);
  case vctrs_type_s3:          return s3_ptype(x, x_arg, call);
  case vctrs_type_scalar:      stop_scalar_type(x, x_arg, call);
  }
  r_stop_unreachable();
}

static
r_obj* col_ptype(r_obj* x) {
  return vec_ptype(x, args_empty, r_lazy_null);
}

static inline
r_obj* vec_ptype_slice(r_obj* x, r_obj* empty) {
  if (r_attrib(x) == r_null) {
    return empty;
  } else {
    // Slicing preserves attributes
    return vec_slice(x, r_null);
  }
}

static
r_obj* s3_ptype(r_obj* x,
               struct vctrs_arg* x_arg,
               struct r_lazy call) {
  switch (class_type(x)) {
  case vctrs_class_bare_tibble:
    return df_ptype(x, true);

  case vctrs_class_data_frame:
    return df_ptype(x, false);

  case vctrs_class_bare_data_frame:
    r_stop_internal("Bare data frames should be handled by `vec_ptype()`.");

  case vctrs_class_none:
    r_stop_internal("Non-S3 classes should be handled by `vec_ptype()`.");

  default:
    break;
  }

  if (vec_is_partial(x)) {
    return x;
  }

  r_obj* method = KEEP(vec_ptype_method(x));

  r_obj* out;

  if (method == r_null) {
    vec_check_vector(x, x_arg, call);
    out = vec_slice(x, r_null);
  } else {
    out = vec_ptype_invoke(x, method);
  }

  FREE(1);
  return out;
}

static inline
r_obj* vec_ptype_method(r_obj* x) {
  r_obj* cls = KEEP(s3_get_class(x));
  r_obj* method = s3_class_find_method("vec_ptype", cls, vctrs_method_table);
  FREE(1);
  return method;
}

static inline
r_obj* vec_ptype_invoke(r_obj* x, r_obj* method) {
  return vctrs_dispatch1(syms_vec_ptype, method, syms_x, x);
}

r_obj* df_ptype(r_obj* x, bool bare) {
  r_obj* row_nms = KEEP(df_rownames(x));

  r_obj* ptype = r_null;
  if (bare) {
    ptype = KEEP(bare_df_map(x, &col_ptype));
  } else {
    ptype = KEEP(df_map(x, &col_ptype));
  }

  if (r_typeof(row_nms) == R_TYPE_character) {
    r_attrib_poke(ptype, r_syms.row_names, vctrs_shared_empty_chr);
  }

  FREE(2);
  return ptype;
}


// [[ register() ]]
r_obj* vec_ptype_finalise(r_obj* x) {
  if (x == r_null) {
    return x;
  }

  // TODO! Error call

  if (!r_is_object(x)) {
    vec_check_vector(x, args_empty, r_lazy_null);
    return x;
  }

  if (vec_is_unspecified(x)) {
    return vec_ptype_finalise_unspecified(x);
  }

  if (vec_is_partial(x)) {
    return vec_ptype_finalise_dispatch(x);
  }

  vec_check_vector(x, args_empty, r_lazy_null);

  switch (class_type(x)) {
  case vctrs_class_bare_tibble:
  case vctrs_class_bare_data_frame:
    return bare_df_map(x, &vec_ptype_finalise);

  case vctrs_class_data_frame:
    x = KEEP(df_map(x, &vec_ptype_finalise));

    if (r_inherits(x, "vctrs:::df_fallback")) {
      r_obj* seen_tibble_attr = KEEP(r_attrib_get(x, r_sym("seen_tibble")));
      bool seen_tibble = r_is_true(seen_tibble_attr);
      FREE(1);

      if (seen_tibble) {
        r_attrib_poke_class(x, classes_tibble);
      } else {
        r_attrib_poke_class(x, classes_data_frame);
      }

      r_attrib_poke(x, r_sym("known_classes"), r_null);
      r_attrib_poke(x, r_sym("seen_tibble"), r_null);
    }

    FREE(1);
    return x;

  case vctrs_class_none:
    r_stop_internal("Non-S3 classes should have returned by now.");

  default:
    return vec_ptype_finalise_dispatch(x);
  }
}

static
r_obj* vec_ptype_finalise_unspecified(r_obj* x) {
  r_ssize size = Rf_length(x);

  if (size == 0) {
    return vctrs_shared_empty_lgl;
  }

  r_obj* out = KEEP(r_alloc_logical(size));
  r_lgl_fill(out, r_globals.na_lgl, size);

  FREE(1);
  return out;
}

static
r_obj* vec_ptype_finalise_dispatch(r_obj* x) {
  return vctrs_dispatch1(
    syms_vec_ptype_finalise_dispatch, fns_vec_ptype_finalise_dispatch,
    syms_x, x
  );
}

void vctrs_init_ptype(r_obj* ns) {
  syms_vec_ptype = r_sym("vec_ptype");

  syms_vec_ptype_finalise_dispatch = r_sym("vec_ptype_finalise_dispatch");
  fns_vec_ptype_finalise_dispatch = r_eval(syms_vec_ptype_finalise_dispatch, ns);
}

static
r_obj* syms_vec_ptype = NULL;

static
r_obj* syms_vec_ptype_finalise_dispatch = NULL;

static
r_obj* fns_vec_ptype_finalise_dispatch = NULL;
