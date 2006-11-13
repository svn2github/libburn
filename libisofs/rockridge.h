/* vim: set noet ts=8 sts=8 sw=8 : */

/** Functions and structures used for Rock Ridge support. */

#ifndef ISO_ROCKRIDGE_H
#define ISO_ROCKRIDGE_H

struct ecma119_write_target;
struct ecma119_tree_node;

void rrip_add_PX(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_PN(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_SL(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_NM(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_CL(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_RE(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_TF(struct ecma119_write_target *, struct ecma119_tree_node *);

/* This is special because it doesn't modify the susp fields of the directory
 * that gets passed to it; it modifies the susp fields of the ".." entry in
 * that directory. */
void rrip_add_PL(struct ecma119_write_target *, struct ecma119_tree_node *);

void rrip_finalize(struct ecma119_write_target *, struct ecma119_tree_node *);

#endif /* ISO_ROCKRIDGE_H */
