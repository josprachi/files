/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exo-tree-view.h"
#include "eel-i18n.h"

static gboolean
exo_noop_false (void)
{
    return FALSE;
}
/*
   extern __typeof (exo_noop_false) IA__exo_noop_false __attribute((visibility("hidden"))) G_GNUC_PURE;
#define exo_noop_false IA__exo_noop_false
*/
#define I_(string) (g_intern_static_string ((string)))

/**
 * SECTION: exo-tree-view
 * @title: ExoTreeView
 * @short_description: An improved version of #GtkTreeView
 * @include: exo/exo.h
 *
 * The #ExoTreeView class derives from #GtkTreeView and extends it with
 * the ability to activate rows using single button clicks instead of
 * the default double button clicks. It also works around a few shortcomings
 * of #GtkTreeView, i.e. #ExoTreeView allows the user to drag around multiple
 * selected rows.
**/



#define EXO_TREE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                                                     EXO_TYPE_TREE_VIEW, ExoTreeViewPrivate))



/* Property identifiers */
enum
{
    PROP_0,
    PROP_SINGLE_CLICK
};

/* Signals */
enum
{
    ITEM_HOVERED,
    LAST_SIGNAL
};


static void     exo_tree_view_finalize                      (GObject          *object);
static void     exo_tree_view_get_property                  (GObject          *object,
                                                             guint             prop_id,
                                                             GValue           *value,
                                                             GParamSpec       *pspec);
static void     exo_tree_view_set_property                  (GObject          *object,
                                                             guint             prop_id,
                                                             const GValue     *value,
                                                             GParamSpec       *pspec);
static gboolean exo_tree_view_button_press_event            (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static gboolean exo_tree_view_button_release_event          (GtkWidget        *widget,
                                                             GdkEventButton   *event);
static gboolean exo_tree_view_motion_notify_event           (GtkWidget        *widget,
                                                             GdkEventMotion   *event);
static gboolean exo_tree_view_leave_notify_event            (GtkWidget        *widget,
                                                             GdkEventCrossing *event);
static void     exo_tree_view_drag_begin                    (GtkWidget        *widget,
                                                             GdkDragContext   *context);
static gboolean exo_tree_view_move_cursor                   (GtkTreeView      *view,
                                                             GtkMovementStep   step,
                                                             gint              count);

struct _ExoTreeViewPrivate
{
    /* whether the next button-release-event should emit "row-activate" */
    guint        button_release_activates : 1;

    /* whether drag and drop must be re-enabled on button-release-event (rubberbanding active) */
    guint        button_release_unblocks_dnd : 1;

    /* whether rubberbanding must be re-enabled on button-release-event (drag and drop active) */
    guint        button_release_enables_rubber_banding : 1;

    /* single click mode */
    guint        single_click : 1;

    /* the path below the pointer or NULL */
    GtkTreePath *hover_path;
};

static guint tree_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ExoTreeView, exo_tree_view, GTK_TYPE_TREE_VIEW)



static void
exo_tree_view_class_init (ExoTreeViewClass *klass)
{
    GtkTreeViewClass *gtktree_view_class;
    GtkWidgetClass   *gtkwidget_class;
    GObjectClass     *gobject_class;

    /* add our private data to the class */
    g_type_class_add_private (klass, sizeof (ExoTreeViewPrivate));

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = exo_tree_view_finalize;
    gobject_class->get_property = exo_tree_view_get_property;
    gobject_class->set_property = exo_tree_view_set_property;

    gtkwidget_class = GTK_WIDGET_CLASS (klass);
    gtkwidget_class->button_press_event = exo_tree_view_button_press_event;
    gtkwidget_class->button_release_event = exo_tree_view_button_release_event;
    gtkwidget_class->motion_notify_event = exo_tree_view_motion_notify_event;
    gtkwidget_class->leave_notify_event = exo_tree_view_leave_notify_event;
    gtkwidget_class->drag_begin = exo_tree_view_drag_begin;

    gtktree_view_class = GTK_TREE_VIEW_CLASS (klass);
    gtktree_view_class->move_cursor = exo_tree_view_move_cursor;

    /* initialize the library's i18n support */
    //_exo_i18n_init ();

    /**
     * ExoTreeView:single-click:
     *
     * %TRUE to activate items using a single click instead of a
     * double click.
     *
     * Since: 0.3.1.3
    **/
    g_object_class_install_property (gobject_class,
                                     PROP_SINGLE_CLICK,
                                     g_param_spec_boolean ("single-click",
                                                           _("Single Click"),
                                                           _("Whether the items in the view can be activated with single clicks"),
                                                           TRUE,
                                                           G_PARAM_READWRITE));

    /**
     * ExoTreeView::item-hovered:
     * @treeview: the object on which the signal is emitted
     * @path: the #GtkTreePath for the activated item
     *
     * The ::item-hovered signal is emitted when an item is hovered by the mouse.
     */
    tree_view_signals[ITEM_HOVERED] =
        g_signal_new (I_("item-hovered"),
                      G_TYPE_FROM_CLASS (gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ExoTreeViewClass, item_hovered),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOXED,
                      G_TYPE_NONE, 1,
                      GTK_TYPE_TREE_PATH);

}



static void
exo_tree_view_init (ExoTreeView *tree_view)
{
    /* grab a pointer on the private data */
    tree_view->priv = EXO_TREE_VIEW_GET_PRIVATE (tree_view);
}



static void
exo_tree_view_finalize (GObject *object)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (object);

    /* be sure to release the hover path */
    gtk_tree_path_free (tree_view->priv->hover_path);

    (*G_OBJECT_CLASS (exo_tree_view_parent_class)->finalize) (object);
}



static void
exo_tree_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (object);

    switch (prop_id)
    {
    case PROP_SINGLE_CLICK:
        g_value_set_boolean (value, exo_tree_view_get_single_click (tree_view));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
exo_tree_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (object);

    switch (prop_id)
    {
    case PROP_SINGLE_CLICK:
        exo_tree_view_set_single_click (tree_view, g_value_get_boolean (value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static gboolean
exo_tree_view_button_press_event (GtkWidget      *widget,
                                  GdkEventButton *event)
{
    GtkTreeSelection *selection;
    ExoTreeView      *tree_view = EXO_TREE_VIEW (widget);
    GtkTreePath      *path = NULL;
    gboolean          result;
    GList            *selected_paths = NULL;
    GList            *lp;
    gpointer          drag_data;

    /* by default we won't emit "row-activated" on button-release-events */
    tree_view->priv->button_release_activates = FALSE;

    /* grab the tree selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

    /* check if the button press was on the internal tree view window */
    if (G_LIKELY (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view))))
    {
        /* determine the path at the event coordinates */
        gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL);

        /* we unselect all selected items if the user clicks on an empty
         * area of the tree view and no modifier key is active.
         */
        if (path == NULL && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
            gtk_tree_selection_unselect_all (selection);

        /* completely ignore double-clicks in single-click mode */
        if (tree_view->priv->single_click && event->type == GDK_2BUTTON_PRESS)
        {
            /* make sure we ignore the GDK_BUTTON_RELEASE
             * event for this GDK_2BUTTON_PRESS event.
             */
            gtk_tree_path_free (path);
            return TRUE;
        }

        /* check if the next button-release-event should activate the selected row (single click support) */
        tree_view->priv->button_release_activates = (tree_view->priv->single_click && event->type == GDK_BUTTON_PRESS && event->button == 1
                                                     && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0);
    }

    /* unfortunately GtkTreeView will unselect rows except the clicked one,
     * which makes dragging from a GtkTreeView problematic. That's why we
     * remember the selected paths here and restore them later.
     */
    if (event->type == GDK_BUTTON_PRESS && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0
        && path != NULL && GTK_IS_TREE_SELECTION (selection) && gtk_tree_selection_path_is_selected (selection, path))
    {
        /* if no custom select function is set, we simply use exo_noop_false here,
         * to tell the tree view that it may not alter the selection.
         */
        if (G_LIKELY (gtk_tree_selection_get_select_function (selection) == NULL))
            gtk_tree_selection_set_select_function (selection, (GtkTreeSelectionFunc) exo_noop_false, NULL, NULL);
        else
            selected_paths = gtk_tree_selection_get_selected_rows (selection, NULL);
    }

    /* Rubberbanding in GtkTreeView 2.9.0 and above is rather buggy, unfortunately, and
     * doesn't interact properly with GTKs own DnD mechanism. So we need to block all
     * dragging here when pressing the mouse button on a not yet selected row if
     * rubberbanding is active, or disable rubberbanding when starting a drag.
     */
    if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE
        && gtk_tree_view_get_rubber_banding (GTK_TREE_VIEW (tree_view))
        && event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
        /* check if clicked on empty area or on a not yet selected row */
        if (G_LIKELY (path == NULL || !gtk_tree_selection_path_is_selected (selection, path)))
        {
            /* need to disable drag and drop because we're rubberbanding now */
            drag_data = g_object_get_data (G_OBJECT (tree_view), I_("gtk-site-data"));
            if (G_LIKELY (drag_data != NULL))
            {
                g_signal_handlers_block_matched (G_OBJECT (tree_view),
                                                 G_SIGNAL_MATCH_DATA,
                                                 0, 0, NULL, NULL,
                                                 drag_data);
            }

            /* remember to re-enable drag and drop later */
            tree_view->priv->button_release_unblocks_dnd = TRUE;
        }
        else
        {
            /* need to disable rubberbanding because we're dragging now */
            gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (tree_view), FALSE);

            /* remember to re-enable rubberbanding later */
            tree_view->priv->button_release_enables_rubber_banding = TRUE;
        }
    }

    /* call the parent's button press handler */
    result = (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->button_press_event) (widget, event);

    /* Renew the selection in case parent button press handler invalidates it */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

    /* restore previous selection if the path is still selected */
    if (event->type == GDK_BUTTON_PRESS && (event->state & gtk_accelerator_get_default_mod_mask ()) == 0
        && path != NULL && GTK_IS_TREE_SELECTION (selection) && gtk_tree_selection_path_is_selected (selection, path))
    {
        /* check if we have to restore paths */
        if (G_LIKELY (gtk_tree_selection_get_select_function (selection) != (GtkTreeSelectionFunc) exo_noop_false))
        {
            /* select all previously selected paths */
            for (lp = selected_paths; lp != NULL; lp = lp->next)
                gtk_tree_selection_select_path (selection, lp->data);
        }
    }

    /* see bug http://bugzilla.xfce.org/show_bug.cgi?id=6230 for more information */
    if (G_LIKELY (GTK_IS_TREE_SELECTION (selection) && gtk_tree_selection_get_select_function (selection) == (GtkTreeSelectionFunc) exo_noop_false))
    {
        /* just reset the select function (previously set to exo_noop_false),
         * there's no clean way to do this, so what the heck.
         */
        gtk_tree_selection_set_select_function (selection, NULL, NULL, NULL);
    }

    /* release the path (if any) */
    gtk_tree_path_free (path);

    /* release the selected paths list */
    g_list_foreach (selected_paths, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected_paths);

    return result;
}



static gboolean
exo_tree_view_button_release_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
    GtkTreeViewColumn *column;
    GtkTreeSelection  *selection;
    GtkTreePath       *path;
    ExoTreeView       *tree_view = EXO_TREE_VIEW (widget);
    gpointer           drag_data;
    int expander_size, horizontal_separator;
    gboolean is_expander, on_expander;

    /* verify that the release event is for the int
       ernal tree view window */
    if (G_LIKELY (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view))))
    {
        /* check if we're in single-click mode and the button-release-event should emit a "row-activate" */
        if (G_UNLIKELY (tree_view->priv->single_click && tree_view->priv->button_release_activates))
        {
            /* reset the "release-activates" flag */
            tree_view->priv->button_release_activates = FALSE;

            /* determine the path to the row that should be activated */
            if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, &column, NULL, NULL))
            {
                /* dont activate row if we are on an expander */
                gtk_widget_style_get (widget,
                                      "expander-size", &expander_size,
                                      "horizontal-separator", &horizontal_separator,
                                      NULL);
                is_expander = TRUE;
                if (is_expander) {
                    expander_size += 4;
                    /*printf ("///// exp_size: %d depth_path: %d horiz_sep: %d\n",
                      expander_size, gtk_tree_path_get_depth (path),
                      horizontal_separator);*/
                    on_expander = (event->x <= horizontal_separator / 2 +
                                   gtk_tree_path_get_depth (path) * expander_size);
                } else {
                    on_expander = FALSE;
                }

                if (!on_expander) {
                    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
                    if (gtk_tree_selection_path_is_selected (selection, path))
                    {
                        gtk_tree_selection_unselect_all (selection);
                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, column, FALSE);
                    }
                    /* emit row-activated for the determined row */
                    gtk_tree_view_row_activated (GTK_TREE_VIEW (tree_view), path, column);
                }

                /* cleanup */
                gtk_tree_path_free (path);
            }
        }
        else if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0 && !tree_view->priv->button_release_unblocks_dnd)
        {
            /* determine the path on which the button was released and select only this row, this way
             * the user is still able to alter the selection easily if all rows are selected.
             */
            if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, &column, NULL, NULL))
            {
                /* check if the path is selected */
                selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
                if (gtk_tree_selection_path_is_selected (selection, path))
                {
                    /* unselect all rows */
                    gtk_tree_selection_unselect_all (selection);

                    /* select the row and place the cursor on it */
                    gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, column, FALSE);
                }

                /* cleanup */
                gtk_tree_path_free (path);
            }
        }
    }

    /* check if we need to re-enable drag and drop */
    if (G_LIKELY (tree_view->priv->button_release_unblocks_dnd))
    {
        drag_data = g_object_get_data (G_OBJECT (tree_view), I_("gtk-site-data"));
        if (G_LIKELY (drag_data != NULL))
        {
            g_signal_handlers_unblock_matched (G_OBJECT (tree_view),
                                               G_SIGNAL_MATCH_DATA,
                                               0, 0, NULL, NULL,
                                               drag_data);
        }
        tree_view->priv->button_release_unblocks_dnd = FALSE;
    }

    /* check if we need to re-enable rubberbanding */
    if (G_UNLIKELY (tree_view->priv->button_release_enables_rubber_banding))
    {
        gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (tree_view), TRUE);
        tree_view->priv->button_release_enables_rubber_banding = FALSE;
    }

    /* call the parent's button release handler */
    return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->button_release_event) (widget, event);
}


static void
exo_tree_view_item_hovered (ExoTreeView      *tree_view,
                            GtkTreePath      *path)
{
    g_return_if_fail (EXO_IS_TREE_VIEW (tree_view));

    g_signal_emit (tree_view, tree_view_signals[ITEM_HOVERED], 0, path);
}

static gboolean
exo_tree_view_motion_notify_event (GtkWidget      *widget,
                                   GdkEventMotion *event)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (widget);
    GtkTreePath *path = NULL;
    GdkCursor   *cursor;

    /* check if the event occurred on the tree view internal window and we are in single-click mode */
    if (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree_view)) && tree_view->priv->single_click)
    {
        /* check if we're doing a rubberband selection right now (which means DnD is blocked) */
        if (G_UNLIKELY (tree_view->priv->button_release_unblocks_dnd))
        {
            /* we're doing a rubberband selection, so don't activate anything */
            tree_view->priv->button_release_activates = FALSE;
        }
        else
        {
            /* determine the path at the event coordinates */
            gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view), event->x, event->y, &path, NULL, NULL, NULL);

            /* check if we have a new path */
            if ((path == NULL && tree_view->priv->hover_path != NULL) ||
                (path != NULL && tree_view->priv->hover_path == NULL) ||
                (path != NULL && tree_view->priv->hover_path != NULL && gtk_tree_path_compare (path, tree_view->priv->hover_path) != 0))
            {
                /* release the previous hover path */
                gtk_tree_path_free (tree_view->priv->hover_path);

                /* setup the new path */
                tree_view->priv->hover_path = path;
                exo_tree_view_item_hovered (tree_view, path);
            }
            else
            {
                /* release the path resources */
                gtk_tree_path_free (path);
            }
        }
    }

    /* call the parent's motion notify handler */
    return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->motion_notify_event) (widget, event);
}



static gboolean
exo_tree_view_leave_notify_event (GtkWidget        *widget,
                                  GdkEventCrossing *event)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (widget);

    /* release and reset the hover path (if any) */
    gtk_tree_path_free (tree_view->priv->hover_path);
    tree_view->priv->hover_path = NULL;
    exo_tree_view_item_hovered (tree_view, NULL);

    /* the next button-release-event should not activate */
    tree_view->priv->button_release_activates = FALSE;

    /* call the parent's leave notify handler */
    return (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->leave_notify_event) (widget, event);
}



static void
exo_tree_view_drag_begin (GtkWidget      *widget,
                          GdkDragContext *context)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (widget);

    /* the next button-release-event should not activate */
    tree_view->priv->button_release_activates = FALSE;

    /* call the parent's drag begin handler */
    (*GTK_WIDGET_CLASS (exo_tree_view_parent_class)->drag_begin) (widget, context);
}



static gboolean
exo_tree_view_move_cursor (GtkTreeView    *view,
                           GtkMovementStep step,
                           gint            count)
{
    ExoTreeView *tree_view = EXO_TREE_VIEW (view);

    /* release and reset the hover path (if any) */
    gtk_tree_path_free (tree_view->priv->hover_path);
    tree_view->priv->hover_path = NULL;

    /* call the parent's handler */
    return (*GTK_TREE_VIEW_CLASS (exo_tree_view_parent_class)->move_cursor) (view, step, count);
}



/**
 * exo_tree_view_new:
 *
 * Allocates a new #ExoTreeView instance.
 *
 * Returns: the newly allocated #ExoTreeView.
 *
 * Since: 0.3.1.3
**/
GtkWidget*
exo_tree_view_new (void)
{
    return g_object_new (EXO_TYPE_TREE_VIEW, NULL);
}



/**
 * exo_tree_view_get_single_click:
 * @tree_view : an #ExoTreeView.
 *
 * Returns %TRUE if @tree_view is in single-click mode, else %FALSE.
 *
 * Returns: whether @tree_view is in single-click mode.
 *
 * Since: 0.3.1.3
**/
gboolean
exo_tree_view_get_single_click (ExoTreeView *tree_view)
{
    g_return_val_if_fail (EXO_IS_TREE_VIEW (tree_view), FALSE);
    return tree_view->priv->single_click;
}



/**
 * exo_tree_view_set_single_click:
 * @tree_view    : an #ExoTreeView.
 * @single_click : %TRUE to use single-click for @tree_view, %FALSE otherwise.
 *
 * If @single_click is %TRUE, @tree_view will use single-click mode, else
 * the default double-click mode will be used.
 *
 * Since: 0.3.1.3
**/
void
exo_tree_view_set_single_click (ExoTreeView *tree_view,
                                gboolean     single_click)
{
    g_return_if_fail (EXO_IS_TREE_VIEW (tree_view));

    if (tree_view->priv->single_click != !!single_click)
    {
        tree_view->priv->single_click = !!single_click;
        g_object_notify (G_OBJECT (tree_view), "single-click");
    }
}




GtkTreePath *
exo_tree_view_get_hover_path (ExoTreeView *tree_view)
{
    g_return_val_if_fail (EXO_IS_TREE_VIEW (tree_view), NULL);
    return tree_view->priv->hover_path;
}



#define __EXO_TREE_VIEW_C__