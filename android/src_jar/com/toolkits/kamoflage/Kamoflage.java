/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Bjorling
 * Copyright (C) 2010 by Anton Persson
 *
 * http://www.733kru.org/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

package com.toolkits.kamoflage;

import android.app.AlertDialog;
import android.app.Dialog;

import java.lang.Math;
import android.util.Log;
import android.view.View;
import android.graphics.Paint;
import android.graphics.Canvas;
//import android.widget;
import java.io.InputStream;
import android.content.Context;
import java.util.HashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import java.lang.Integer;

import android.view.GestureDetector;

import android.widget.ProgressBar;
import java.lang.Runnable;

public class Kamoflage
{	
	static final boolean DEBUG_LAYOUT = false;

	static android.app.Activity kamoflage_context;
	static int listrow_resource_id; // needed to create lists
	static int listlayout_resource_id; // needed to create lists

	static float w_ppi, h_ppi;

	public static float get_w_ppi() { return w_ppi; }
	public static float get_h_ppi() { return h_ppi; }
	
	public Kamoflage(android.app.Activity cntxt, int listrow_rid, int listlayout_rid) {
		kamoflage_context = cntxt;
		listrow_resource_id = listrow_rid;
		listlayout_resource_id = listlayout_rid;
		widgets = new HashMap<String, Widget>();

		android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
		cntxt.getWindowManager().getDefaultDisplay().getMetrics(metrics);

		android.view.ViewConfiguration config = android.view.ViewConfiguration.get(cntxt);
		
		float w = (float)(metrics.widthPixels) / metrics.xdpi;
		float h = (float)(metrics.heightPixels) / metrics.ydpi;
		float d = (float)Math.sqrt((double)(w * w + h * h));
		float w_p = (float)(metrics.widthPixels);
		float h_p = (float)(metrics.heightPixels);
		float d_p = (float)Math.sqrt((double)(w_p * w_p + h_p * h_p));

		w_ppi = w_p / w;
		h_ppi = h_p / h;
		
		setDisplayConfiguration(w, h, metrics.xdpi, metrics.ydpi, config.getScaledEdgeSlop());

	}
	
	private static HashMap<String, Widget> widgets;
	public static class Widget {

		public boolean topLevelWidget = true; // this MUST be set to false when the Widget is put in a another containing Widget
		
		protected long nativeID;
		protected View internal;

		protected boolean expand;
		protected boolean fill;
		protected int border_width;

		protected ContainerBase parent = null;
		
		protected String title;
		protected String id;
		protected String exturi;
		protected String viewtrigger;
		protected Widget viewtrigger_object;

		protected void do_automatic_action() {
			if(viewtrigger != null && (!viewtrigger.equals(""))) {
				viewtrigger_object = get_widget(viewtrigger);
				if(viewtrigger_object != null)
					viewtrigger_object.show_now();
			}
			if(exturi != null && (!exturi.equals(""))) {
				android.content.Intent browserIntent =
					new android.content.Intent("android.intent.action.VIEW",
								   android.net.Uri.parse(exturi));
				kamoflage_context.startActivity(browserIntent);
			}
		}
		
		private static int id_counter = 0;
		static protected String generateID() {
			String result = "__ANDROID_GENERATED_THIS_ID__" + id_counter;
			id_counter++;

			return result;
		}
		
		protected void deregister_self() {
			if(widgets.get(id) == null)			
				throw new RuntimeException(
					"The ID was NOT found - inconcisteny. [" + id + "]");
			widgets.remove(id);
		}
		
		protected void register_id() {
			if(widgets.get(id) != null)
				throw new RuntimeException(
					"The ID is already in use. [" + id + "]");
			widgets.put(id, this);
		}

		// this should be called when you
		// want to set up the internal object
		// the reason is that code HAS to run on
		// the UI thread, this will make sure it does
		// and call your initiating function from that thread
		protected void initiate_internal() {
			final Widget w = this;
			final ThreadSignal ts = new ThreadSignal();
			
			kamoflage_context.runOnUiThread(new Runnable() {
					public void run() {
						w.create_internal_object();
						ts.doSignal();
					}
				});

			ts.doWait();
		}

		protected void calculateMinimumSizeTopLevel() {
			if(topLevelWidget) {
				IntegerPair ignored = calculateMinimumSize();
			}
		}
		
		private int minimumCache_rw = 0;
		private int minimumCache_rh = 0;
		private static String tabbur = "";
		protected IntegerPair calculateMinimumSize() {
			String ctab = tabbur;
			if(DEBUG_LAYOUT) Log.v("Kamoflage", ctab + "TOPLEVEL?: " + id + " = " + topLevelWidget);

			IntegerPair returnValue = new IntegerPair();

			tabbur = ctab + "   ";
			returnValue = calculateMinimumSizeInternal();
			tabbur = ctab;
			
			minimumCache_rw = returnValue.a;
			minimumCache_rh = returnValue.b;

			if(DEBUG_LAYOUT) Log.v("Kamoflage", ctab + "CALCULATE: " + id + " minimum: " + returnValue.a + ", " + returnValue.b);
			
			return returnValue;
		}
		protected IntegerPair getMinimumSize() {
			IntegerPair returnValue = new IntegerPair();
			
			returnValue.a = minimumCache_rw;
			returnValue.b = minimumCache_rh;

			return returnValue;
		}
 		
		private static android.app.Activity act = null;
		protected IntegerPair calculateMinimumSizeInternal() {
			
			int screen_w = 640; // just throw in some defaults
			int screen_h = 480;
			
			if(act == null) act = (android.app.Activity)kamoflage_context;
			if(act != null) {
				android.view.WindowManager wm = act.getWindowManager();
				android.view.Display d = wm.getDefaultDisplay();
				screen_w = d.getWidth();
				screen_h = d.getHeight();
			}
			
			int childWidthSpec, childHeightSpec;

			childWidthSpec =
				android.view.View.MeasureSpec.makeMeasureSpec(
					screen_w,
					android.view.View.MeasureSpec.AT_MOST);					

			childHeightSpec =
				android.view.View.MeasureSpec.makeMeasureSpec(
					screen_h,
					android.view.View.MeasureSpec.AT_MOST);
			
			android.view.View v = internal;

			v.measure(childWidthSpec, childHeightSpec);

			IntegerPair ip = new IntegerPair();
			ip.a = v.getMeasuredWidth();
			ip.b = v.getMeasuredHeight();

			return ip; 
		}

		protected void create_internal_object() {
			/* ignore, this should be overridden */
			throw new RuntimeException(
				"The create_internal_object method was not properly overriden by sub class, this is a bug.");
		}
		
		public void parse_widget(Element widget) {
			title = widget.getAttribute("title");
			id = widget.getAttribute("id");

			viewtrigger = widget.getAttribute("viewtrigger");

			exturi = widget.getAttribute("exturi");
			
			String exp_str = widget.getAttribute("expand");
			String fill_str = widget.getAttribute("fill");
			String bord_str = widget.getAttribute("border");

			expand = exp_str.equals("true") ? true : false;
			fill = fill_str.equals("true") ? true : false;
			try {
				border_width = Integer.valueOf(bord_str);
			} catch (Exception e) {
				border_width = 0;
			}

			register_id();
		}

		public String get_id() {
			return id;
		}

		public void setNativeID(long _nativeID) {
			nativeID = _nativeID;
		}

		public void invalidate_view() {
			internal.invalidate();
		}

		public View getInternal() {
			return internal;
		}

		public boolean get_expand() {
			return expand;
		}
		
		public boolean get_fill() {
			return fill;
		}
		
		public void set_expand(boolean _xp) {
			expand = _xp;
		}
		
		public void set_fill(boolean _fl) {
			fill = _fl;
		}
		
		// show_now_no_push() tells Kamoflage to show this object to the user now. This might require multiple operations by Kamoflage, such as shifting tabs, scrolling through lists and so forth. (please note, this is not fully implemented yet... Currently it will only work on tabs directly controlling this object... no chain reactions..)
		// this method does NOT push the Widget to the stack
		public void show_now_no_push() {
			if(parent == null) return;

			parent.show_child(this);
		}

		// just call show_now_no_push() and then push the widget on the stack after..
		public void show_now() {
			show_now_no_push();
			pushWidgetToStack(this);
		}
	}

	private static java.util.Stack kamoWidgetStack = new java.util.Stack();
	
	private static void pushWidgetToStack(Widget wid) {
		if(kamoWidgetStack.size() == 10) {
			kamoWidgetStack.remove(0);
		}

		kamoWidgetStack.push(wid);
	}

	public static void popWidgetFromStack() {
		if(kamoWidgetStack.size() > 1) {
			kamoWidgetStack.pop();
			Widget wid = (Widget)kamoWidgetStack.peek();
			wid.show_now_no_push();
		}
	}
	
	public static Widget get_widget(String id) {
		return widgets.get(id);
	}
	
	static abstract class ContainerBase extends Widget {
		abstract void clear();
		abstract void add(Widget wid);
		abstract void show_child(Widget child);
		protected void setMinimumSizeDirtyFlag() {
			/* ignore, this should be overridden */
			throw new RuntimeException(
				"The setMinimumSizeDirtyFlag method was not properly overriden by sub class, this is a bug.");
		}
	}

	static class Container extends ContainerBase {
		private boolean framed, scrollable;
		private String align, name;
		private boolean horizontal;
		private KamoflageLayout layout_obj;
		private Vector<Widget> contained;
		
		// called from PARSER
		public Container(String _name, String _align) {
			contained = new Vector<Widget>();
			name = _name;
			align = _align;
			framed = false;
			scrollable = false;

			horizontal = false;
			
			if(align.equals("vertical")) {
				horizontal = false;
			} else {
				horizontal = true;
			}
		}

		// I know, this is plain ugly but I'm desperate to
		// have the issue of MinimumSize calculation solved
		// so that I can go forward and fix stuff that's actually
		// entertaining to code... If you don't like it, curse...
		private static boolean one_run_dummy = false;
		private static Button dummy_bt = new Button();
		public IntegerPair calculateMinimumSizeInternal() {
			final int c_s = contained.size();

			int i;

			int w = 0, h = 0;
			IntegerPair ip = new IntegerPair();
			for(i = 0; i < c_s; i++) {
				Widget cnt = contained.get(i);
				
				ip = cnt.calculateMinimumSize();
				
				if(horizontal) {
					w += ip.a;
					if(ip.b > h)
						h = ip.b;
				} else {
					h += ip.b;
					if(ip.a > w)
						w = ip.a;
				}
			}
			ip.a = w; ip.b = h;

			if(scrollable) {
				if(!one_run_dummy) {
					one_run_dummy = true;
					dummy_bt.set_title("berub");
				}
				ip = dummy_bt.calculateMinimumSizeInternal();
			} 

			return ip;
		}
		
		protected void create_internal_object() {
			layout_obj = new KamoflageLayout(
				this,
				title, kamoflage_context, horizontal, framed, scrollable);
			if(scrollable) {
				android.widget.ScrollView sv =
					new android.widget.ScrollView(kamoflage_context);
				
				sv.addView(layout_obj);
				sv.setFillViewport(true);

				internal = sv;
			} else internal = layout_obj;
		}
		
		public void parse_container(Element widget) {
			String scr_str = widget.getAttribute("scrollable");
			String frmd_str = widget.getAttribute("framed");

			scrollable = scr_str.equals("true") ? true : false;
			framed = frmd_str.equals("true") ? true : false;

			initiate_internal();
		}
		
		// called from APPLICATION code
		public Container(String titleNid, boolean _horizontal) {
			contained = new Vector<Widget>();

			title = titleNid;
			id = titleNid;
			horizontal = _horizontal;

			layout_obj = new KamoflageLayout(
				this, title, kamoflage_context, horizontal, false, false);
			
			internal = layout_obj;

			register_id();

		}
		
		void clear() {
			contained.clear();
			layout_obj.removeAllViews();
		}
		
		void add(Widget wid) {
			contained.add(wid);
			
			wid.parent = this;
			wid.topLevelWidget = false;
			layout_obj.addKWidget(wid);
		}
		
		void show_child(Widget child) {
			if(DEBUG_LAYOUT) Log.v("Kamoflage", "show_child() in class Container not implemented.");
		}
	}

	static class ChainedWidget {
		public ChainedWidget prev, next;
		public Widget wid;
	}

	// this class is just a dummy container so that we
	// can add a KamoflageLayout object to another KamoflageLayout object...
	static class KamoflageLayoutWidget extends Widget {
		public KamoflageLayoutWidget(String kamoflageID, KamoflageLayout kamola) {
			title = id = kamoflageID;
			internal = kamola;
		}
		public int min_w, min_h;
		public IntegerPair getMinimumSize() {
			IntegerPair rv = new IntegerPair();
			rv.a = min_w;
			rv.b = min_h;
			return rv;
		}		
	}

	// This is just a dummy container since we want to be able to add
	// a KamoflageVSwitch object to a KamoflageLayout object... The KamoflageLayout
	// object only accepts Widget objects...
	static class TabsVSContainer extends Widget {
		protected void create_internal_object() {
			internal = new KamoflageVSwitch("tabSwitch", kamoflage_context);
		}
		
		public TabsVSContainer(String kamoflageID) {
			title = id = kamoflageID;
			initiate_internal();
		}

		public int min_w, min_h;
		public IntegerPair getMinimumSize() {
			IntegerPair rv = new IntegerPair();
			rv.a = min_w;
			rv.b = min_h;
			return rv;
		}		
	}
	
	static class Tabs extends ContainerBase implements View.OnClickListener {

		private KamoflageLayoutWidget knavi;
		private TabsVSContainer tvs;
		private KamoflageVSwitch vf;
		private KamoflageLayout base;
		private KamoflageLayout navi;
		private Label tvl = null;
		private android.widget.Button prev, next;
		Button kprev, knext;

		boolean tabs_on_top = true;
		boolean hidetabs = false;

		private ChainedWidget current = null;
		
		void show_child(Widget child) {
			int step = 0;
			ChainedWidget searching_at = current;

			if(searching_at == null) return;

			do {
				if(searching_at.wid == child) {
					if(searching_at == current) return;
					
					current = searching_at;
					vf.showNext(step);
					
					if(tvl != null)
						tvl.set_title(current.wid.title);

					// Update native tab view book keeping
					tabViewChanged(id, current.wid.id);
					
					// trigger an event, so that a
					// listener can detect that the
					// tab changed the view
					handleOnValueChanged(id);
					
					return;
				}
				step++;
				searching_at = searching_at.next;
			} while(searching_at != current);
		}

		public void onClick(View v) {
			if(v == prev) {
				if(current != null)
					current = current.prev;
				vf.showPrevious();
			} else {
				if(current != null)
					current = current.next;
				vf.showNext();
			}
			if(current != null)
				tvl.set_title(current.wid.title);
		}

		public IntegerPair calculateMinimumSizeInternal() {
			int w = 0, h = 0;

			ChainedWidget cw = current;

			IntegerPair ip = new IntegerPair();
			if(cw != null) {
				do {
					ip = cw.wid.calculateMinimumSize();
					
					if(ip.a > w) w = ip.a;
					if(ip.b > h) h = ip.b;
					
					cw = cw.next;
				} while(cw != current);

				tvs.min_w = w;
				tvs.min_h = h;
			}

			if(!hidetabs) {
				IntegerPair ipc = kprev.calculateMinimumSize();
				ip = tvl.calculateMinimumSize();
				ipc.a += ip.a; ipc.b = ip.b > ipc.b ? ip.b : ipc.b;
				ip = knext.calculateMinimumSize();
				ipc.a += ip.a; ipc.b = ip.b > ipc.b ? ip.b : ipc.b;

				knavi.min_w = ipc.a;
				knavi.min_h = ipc.b;
				
				ip.a = w > ipc.a ? w : ip.a; ip.b = h + ipc.b;
			}
			
			return ip;
		}
		
		String name;
		protected void create_internal_object() {
			base = new KamoflageLayout(this, name + ":base", kamoflage_context, false, false, false);
			if(!hidetabs) {
				navi = new KamoflageLayout(this, name + ":navi", kamoflage_context, true, false, false);

				kprev = new Button(name + ":nprev");
				knext = new Button(name + ":nnext");
				
				prev = (android.widget.Button)kprev.getInternal();
				prev.setText("<<");
				next = (android.widget.Button)knext.getInternal();
				next.setText(">>");
				
				prev.setOnClickListener(this);
				next.setOnClickListener(this);

				tvl = new Label(name + ":nlabel");
				tvl.set_expand(true); tvl.set_fill(true);
				
//				tvl = ktvl.getInternal();
//					new android.widget.TextView(kamoflage_context);

				navi.addKWidget(kprev);
				navi.addKWidget(tvl);
				navi.addKWidget(knext);
				kprev.parent = tvl.parent = knext.parent = this;
				kprev.topLevelWidget = tvl.topLevelWidget = knext.topLevelWidget = false;

			}

			tvs = new TabsVSContainer(name + ":tabsvsc");
			vf = (KamoflageVSwitch)tvs.getInternal();
			tvs.set_expand(true); tvs.set_fill(true);

			knavi = new KamoflageLayoutWidget(name + ":navig", navi);
			
			if(!hidetabs) {
				if(tabs_on_top) {
					base.addKWidget(knavi);
					base.addKWidget(tvs);
				} else {
					base.addKWidget(tvs);
					base.addKWidget(knavi);
				}
				tvs.parent = knavi.parent = this;
				tvs.topLevelWidget = knavi.topLevelWidget = false;
			} else {
				base.addKWidget(tvs);

				tvs.parent = this;
				tvs.topLevelWidget = false;
			}
			
			internal = base;
		}
		
		public Tabs(String _name, String _tabs_on_top, String _hidetabs) {
			name = _name;
			expand = true;
			
			if(_tabs_on_top.equals("false")) tabs_on_top = false;
			if(_hidetabs.equals("true")) hidetabs = true;

			initiate_internal();
		}

		void clear() {
		}

		void add(Widget wid) {			
			vf.addKWidget(wid.internal);

			wid.parent = this;
			wid.topLevelWidget = false;

			ChainedWidget cwg = new ChainedWidget();
			cwg.wid = wid;
			if(current == null) {
				cwg.next = cwg.prev = cwg;
				current = cwg;
				// Update native tab view book keeping
				tabViewChanged(id, current.wid.id);
			} else {
				cwg.next = current;
				cwg.prev = current.prev;
				
				current.prev.next = cwg;
				current.prev = cwg;
			}

			if(!hidetabs)
				tvl.set_title(current.wid.title);
		}
	}
	
	static class Window extends Container {
		private String align;
		private boolean fullscreen;
		
		Window(String _align, boolean _fullscreen) {
			super("<window>", _align);
			fullscreen = _fullscreen;
		}
	}

	static class Label extends Widget {

		protected void create_internal_object() {
			internal = new android.widget.TextView(kamoflage_context);
		}
		
		public Label() {
			initiate_internal();
		}

		public Label(String titleNid) {

			title = titleNid;
			id = titleNid;
			
			initiate_internal();

			register_id();
		}

		
		public void parse_label(Element label) {
			((android.widget.TextView)internal).setText(title);
		}
		
		public void set_title(String _title) {
			title = _title;
			((android.widget.TextView)internal).setText(title);
		}
	}
	
	static class Button extends Widget implements View.OnClickListener {

		Button() {
			initiate_internal();
		}

		protected void create_internal_object() {
//			internal = new android.widget.Button(kamoflage_context);
			internal = new ButtonHelper(kamoflage_context);
			((android.widget.Button)internal).setOnClickListener(this);
		}
		
		public Button(String titleNid) {

			title = titleNid;
			id = titleNid;

			initiate_internal();

			register_id();

		}
		
		public void parse_button(Element button) {
			((android.widget.Button)internal).setText(title);
		}
		
		public void onClick(View v) {
			do_automatic_action();
			handleOnClick(id);
		}
		
		public void set_title(String _title) {
			title = _title;
			((android.widget.Button)internal).setText(title);
		}
	}
	
	static class List extends Widget implements
					 android.widget.AdapterView.OnItemSelectedListener,
					 android.widget.AdapterView.OnItemClickListener {

		private Vector<String> titles;

		private int item_selected; //used for the spinner case..
		private ListAdapter lad;		
		private boolean dropdown;

		private class ListAdapter extends android.widget.ArrayAdapter< Vector<String> > {
			Context __ctx;
			int lr_rid;
			int ll_rid;
			boolean set_color_to_black;
			
			public ListAdapter(
				boolean _set_color_to_black, Context ctx, int listRowId, int listLayoutId) {
				super(ctx, listRowId);
				__ctx = ctx;
				lr_rid = listRowId;
				ll_rid = listLayoutId;
				set_color_to_black = _set_color_to_black;
			}

			private View internal_GetView(int position, View convertView, android.view.ViewGroup parent) {
				View v = convertView;
				if(v == null) {
					android.view.LayoutInflater vi = (android.view.LayoutInflater)
						kamoflage_context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
					v = vi.inflate(lr_rid, null);
				}
				
				Vector<String> i = getItem(position);
				if (i != null) {
					android.widget.LinearLayout ll =
						(android.widget.LinearLayout) v.findViewById(ll_rid);
					
					int k;
					for(k = 0; k < i.size(); k++) {
						android.widget.TextView tv;

						tv = (android.widget.TextView)v.findViewById(k);
						if(tv == null) {
							tv =
								new android.widget.TextView(kamoflage_context);
							tv.setId(k);
							if(set_color_to_black)
								tv.setTextColor(0xff00ff);
							ll.addView(tv);
						}
						tv.setText(i.get(k));
						
					}
				}
				return v;
			}
			
			@Override
				public View getView(int position, View convertView, android.view.ViewGroup parent)
			{
				return internal_GetView(position, convertView, parent);
			}

			@Override
				public View getDropDownView (int position, View convertView,
							     android.view.ViewGroup parent)
			{
				return internal_GetView(position, convertView, parent);
			}
		}

		// I know, this is plain ugly but I'm desperate to
		// have the issue of MinimumSize calculation solved
		// so that I can go forward and fix stuff that's actually
		// entertaining to code... If you don't like it, curse...
		private static boolean one_run_dummy = false;
		private static Button dummy_bt = new Button();
		public IntegerPair calculateMinimumSizeInternal() {
			int i;

			int w = 0, h = 0;
			IntegerPair ip = new IntegerPair();

			if(!one_run_dummy) {
				one_run_dummy = true;
				dummy_bt.set_title("berub");
			}
			ip = dummy_bt.calculateMinimumSizeInternal();
			
			return ip;
		}

		
		List() {
			item_selected = 0;
			dropdown = false;
		}

		protected void create_internal_object() {
			if(!dropdown) {
				lad = new ListAdapter(false, kamoflage_context, listrow_resource_id, listlayout_resource_id);
				android.widget.ListView lv = new android.widget.ListView(kamoflage_context);
				lv.setAdapter(lad);
				
				lv.setChoiceMode(android.widget.ListView.CHOICE_MODE_SINGLE);
				
				internal = lv;
//				lv.setOnItemSelectedListener(this);
				lv.setOnItemClickListener(this);
			} else {
				lad = new ListAdapter(false, kamoflage_context, listrow_resource_id, listlayout_resource_id);
				android.widget.Spinner sv = new android.widget.Spinner(kamoflage_context);
				sv.setAdapter(lad);

				item_selected = 0;
				internal = sv;
				sv.setOnItemSelectedListener(this);
//				sv.setOnItemClickListener(this);
			}
		}
		
		public void parse_list(Element list) {
			String drpd_s = list.getAttribute("isdropdown");
			if(drpd_s.equals("true")) {
				dropdown = true;
			}

			titles = new Vector<String>();			

			NodeList nodes = list.getChildNodes();
			for (int i = 0; i < nodes.getLength(); i++) {
				Node n = nodes.item(i);
				if(n.getNodeName().equals("column")) {
					String t = ((Element)n).getAttribute("title");

					titles.add(t);
				}
			}

			initiate_internal();
		}

		public void onNothingSelected(android.widget.AdapterView parent) {
		}
		
		public void onItemSelected(android.widget.AdapterView parent,
					   android.view.View view, int position, long _id) {
			item_selected = position;
			handleOnSelectRow(id, position);
		}
		
		public void onItemClick(android.widget.AdapterView parent,
					android.view.View view, int position, long _id) {
			item_selected = position;
			handleOnSelectRow(id, position);
		}

		void add_row(Vector<String> data) {
			lad.add(data);
		}
		
		void remove_row(int sel) {
			lad.remove(lad.getItem(sel));
		}
		
		void clear() {
			if(dropdown) {
				((android.widget.Spinner)internal).setSelection(0);
			}
			lad.clear();
			item_selected = -1;
		}

		int get_selected() {
			if(!dropdown) {
				return ((android.widget.ListView)internal).getCheckedItemPosition();
			}
			return item_selected;
			
		}

		String get_value(int sel, int col) {
			Vector<String> i = lad.getItem(sel);
			if(i != null) {
				return i.get(col);
			}
			return("");
		}
	}
	
	static class Entry extends Widget implements android.text.TextWatcher, 
			   android.widget.TextView.OnEditorActionListener {
		Entry() {
			initiate_internal();
		}

		protected void create_internal_object() {
			internal = new android.widget.EditText(kamoflage_context);
			((android.widget.EditText)internal).setSingleLine(true);
			((android.widget.EditText)internal).addTextChangedListener(this);
//			((android.widget.EditText)internal).setOnEditorActionListener(this);
		}
		
		public void afterTextChanged(android.text.Editable s) {
			if(skip_action) {
				return;
			}
			handleOnModify(id);
		}

		public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
		public void onTextChanged(CharSequence s, int start, int count, int after) {}

		boolean skip_action = false;
		
		public boolean onEditorAction(android.widget.TextView v, int actionId, android.view.KeyEvent event) {
			switch(actionId) {
			case android.view.inputmethod.EditorInfo.IME_ACTION_DONE:
			case android.view.inputmethod.EditorInfo.IME_ACTION_NEXT:
			case android.view.inputmethod.EditorInfo.IME_ACTION_GO:
			case android.view.inputmethod.EditorInfo.IME_ACTION_UNSPECIFIED:
				handleOnModify(id);
				return true;
			}
			if(DEBUG_LAYOUT) Log.v("Kamoflage", "Entry reported unknown action![" + Integer.toString(actionId) + "]");
			return false;
		}
		
		public void set_text(String _txt) {
			skip_action = true;
			((android.widget.EditText)internal).setText(_txt.subSequence(0, _txt.length()));
			skip_action = false;
		}

		public String get_text() {
			return (((android.widget.EditText)internal).getText()).toString();
		}
	}

	// we call this KamoCanvas, but it is used by the native KammoGUI::Canvas class..
	static class KamoCanvas extends Widget {

		public KamoCanvas() {
		}

		protected void create_internal_object() {
			internal = new CanvasHelper(nativeID, id, kamoflage_context);
		}
		
		public void parse_canvas(Element canvas) {
			initiate_internal();
		}

		public void start_animation() {
			CanvasHelper h = (CanvasHelper)internal;

			h.start_animation();			
		}

		public void stop_animation() {
			CanvasHelper h = (CanvasHelper)internal;

			h.stop_animation();			
		}
	}
	
	static class SVGCanvas extends Widget {

		public SVGCanvas() {
		}

		protected void create_internal_object() {
			internal = new SVGCanvasHelper(nativeID, id, kamoflage_context);
		}
		
		public void parse_svg_canvas(Element canvas) {
			initiate_internal();
		}

		public void start_animation() {
			SVGCanvasHelper h = (SVGCanvasHelper)internal;

			h.start_animation();			
		}

		public void stop_animation() {
			SVGCanvasHelper h = (SVGCanvasHelper)internal;

			h.stop_animation();			
		}
	}
	
	static class Surface extends Widget {

		private int horsec = 16, versec = 16, hordiv = 1, verdiv = 16;

		public Surface() {
		}

		protected void create_internal_object() {
			internal = new SurfaceHelper(
				id,
				kamoflage_context,
				horsec, versec, hordiv, verdiv);
		}
		
		public void parse_surface(Element surface) {			
			String hrzs_s = surface.getAttribute("horizontal");
			String vers_s = surface.getAttribute("vertical");
			String hrzd_s = surface.getAttribute("horizontal_div");
			String verd_s = surface.getAttribute("vertical_div");
			try {
				horsec = Integer.valueOf(hrzs_s);
			} catch (Exception e) {
				horsec = 16;
			}
			try {
				versec = Integer.valueOf(vers_s);
			} catch (Exception e) {
				versec = 16;
			}
			try {
				hordiv = Integer.valueOf(hrzd_s);
			} catch (Exception e) {
				hordiv = 1;
			}
			try {
				verdiv = Integer.valueOf(verd_s);
			} catch (Exception e) {
				verdiv = 16;
			}

			initiate_internal();
		}
	}

	static class CheckButton extends Widget implements View.OnClickListener {

		CheckButton() {
		}

		protected void create_internal_object() {
			internal = new android.widget.CheckBox(kamoflage_context);
			((android.widget.CheckBox)internal).setOnClickListener(this);
			((android.widget.CheckBox)internal).setText(title);
		}

		public void parse_checkbutton(Element xbutton) {
			initiate_internal();
		}
		
		public void onClick(View v) {
			handleOnClick(id);
		}
		
		public boolean get_state() {
			return ((android.widget.CheckBox)internal).isChecked();
		}
		
		public void set_state(boolean state) {
			((android.widget.CheckBox)internal).setChecked(state);
		}
	}
	
	public static class Scale extends Widget implements android.widget.SeekBar.OnSeekBarChangeListener {
		private double min, max, inc;
		private android.widget.SeekBar sbar;
		Scale() {
		}

		protected void create_internal_object() {
			android.widget.LinearLayout layob =
				new android.widget.LinearLayout(kamoflage_context);

			layob.setOrientation(android.widget.LinearLayout.VERTICAL);
			layob.setGravity(android.view.Gravity.CENTER_VERTICAL);
			
			sbar = new android.widget.SeekBar(kamoflage_context);
			sbar.setOnSeekBarChangeListener(this);
			sbar.setMax((int)((max - min) / inc));

			layob.addView(sbar);
			sbar.setVisibility(View.VISIBLE);
			
			internal = layob;
		}			
		
		public Scale(String titleNid, boolean _hrz, double _min, double _max, double _inc) {

			title = titleNid;
			id = titleNid;
			
			min = _min;
			max = _max;
			inc = _inc;

			initiate_internal();

			register_id();

		}
		
		public void parse_scale(Element scale) {
			String hrz_s = scale.getAttribute("horizontal");
			String min_s = scale.getAttribute("min");
			String max_s = scale.getAttribute("max");
			String inc_s = scale.getAttribute("inc");
			try {
				min = Double.valueOf(min_s);
			} catch (Exception e) {
				min = 0;
			}
			try {
				max = Double.valueOf(max_s);
			} catch (Exception e) {
				max = 1.0;
			}
			try {
				inc = Double.valueOf(inc_s);
			} catch (Exception e) {
				inc = 0.1;
			}

			initiate_internal();
		}
		
		public void onProgressChanged(android.widget.SeekBar sb,
					     int change, boolean fromUser) {
			if(fromUser)
				handleOnValueChanged(id);
		}

		public void onStartTrackingTouch(android.widget.SeekBar seekBar) {}
		public void onStopTrackingTouch(android.widget.SeekBar seekBar) {}
		
		public void set_value(double val) {
			if(val >= min && val <= max) {
				double newval = (val - min) / inc;
				sbar.setProgress((int)newval);
			}
		}

		public double get_value() {
			double crnt = sbar.getProgress();
			return crnt * inc + min;
		}
	}

	static class PollEventThread extends java.lang.Thread {
		int msec;
		String _id;
		
		PollEventThread(String id, int msec_intervall) {
			super(id);
			_id = id;
			msec = msec_intervall;
		}

		public void run() {
			while(true) {
				try {
					handleOnPoll(_id);
					sleep(msec);
				} catch (InterruptedException e) {}
			}
		}
	}
	
	static class PollEvent extends Widget {
		static Vector<PollEventThread> polthreads;
		
		PollEventThread thr;
		
		PollEvent() {
			if(polthreads == null)
				polthreads = new Vector<PollEventThread>();
		}

		void parse_poll_event(Element pev) {
			String i_s = pev.getAttribute("interval");
			int i = 1000;
			try {
				i = Integer.valueOf(i_s);
			} catch (Exception e) {
				i = 1000;
			}

			thr = new PollEventThread(id, i);
			polthreads.add(thr);
		}
	}
	
	static class UserEvent extends Widget {
		UserEvent() {
		}

		// this is called from the JNI code KammoGUI::EventHandler::trigger_user_event()
		// so that we in Java-land may know about it.
		void trigger_user_event() {
			do_automatic_action();
		}
		
	}

	public static void executeEventOnUI() {
		kamoflage_context.runOnUiThread(new Runnable() {
				public void run() {
					onUiThreadCallback();
				}
			});
	}

	public static boolean isUIThread() {
		if (android.os.Looper.getMainLooper().getThread() == Thread.currentThread()) {
			return true;
		}

		return false;
	}

	public static void showExternalURI(String exturi) {
		if(exturi != null && (!exturi.equals(""))) {
			android.content.Intent browserIntent =
				new android.content.Intent("android.intent.action.VIEW",
							   android.net.Uri.parse(exturi));
			kamoflage_context.startActivity(browserIntent);
		}
	}
		
	private static String dialog_title, dialog_message;
	private static int dialog_identifier_var;
	
	public static final int BUSY_DIALOG = 10;
	public static final int NOTIFICATION_DIALOG = 20;
	public static final int YESNO_DIALOG = 30;
	public static final int CANCELABLE_BUSY_DIALOG = 40;

	public static void showKamoflageDialog(int dialog_type, int identifier_var,
					       String btitle, String bmessage) {
		dialog_title = btitle; dialog_message = bmessage;
		dialog_identifier_var = identifier_var;
		kamoflage_context.showDialog(dialog_type);
	}
		
	public static void hideKamoflageDialog(int _dialog_type) {
		final int dialog_type = _dialog_type;
		kamoflage_context.runOnUiThread(new Runnable() {
				public void run() {
					kamoflage_context.removeDialog(dialog_type);
				}
			});
	}
		
	public static Dialog onCreateDialog(int dialogID) {
		AlertDialog.Builder builder = new AlertDialog.Builder(kamoflage_context);
		builder.setTitle(dialog_title);
		builder.setMessage(dialog_message);

		switch(dialogID) {
		case BUSY_DIALOG:
		{
			return builder.create();
		}
		case CANCELABLE_BUSY_DIALOG:
		{
			builder.setNeutralButton(
			"CANCEL",
			new android.content.DialogInterface.OnClickListener() {
				public void onClick(android.content.DialogInterface dialog, int which) {
					/* ignore */
					kamoflage_context.runOnUiThread(new Runnable() {
							public void run() {
								onCancelableBusyWorkCallback();
								kamoflage_context.removeDialog(CANCELABLE_BUSY_DIALOG);
							}
						});
				}
			}
			);
			
			return builder.create();
		}
		case NOTIFICATION_DIALOG:
		{
			builder.setNeutralButton(
			"OK",
			new android.content.DialogInterface.OnClickListener() {
				public void onClick(android.content.DialogInterface dialog, int which) {
					/* ignore */
					kamoflage_context.runOnUiThread(new Runnable() {
							public void run() {
								kamoflage_context.removeDialog(
									NOTIFICATION_DIALOG);
							}
						});
				}
			}
			);
			
			return builder.create();
		}
		case YESNO_DIALOG:
		{
			builder.setPositiveButton(
				"YES",
				new android.content.DialogInterface.OnClickListener() {
					public void onClick(android.content.DialogInterface dialog, int which) {
						kamoflage_context.runOnUiThread(new Runnable() {
								public void run() {
									yesNoDialogCallback(
										true, dialog_identifier_var);
									kamoflage_context.removeDialog(YESNO_DIALOG);
								}
							});
					}
				}
				);
			builder.setNegativeButton(
				"NO",
				new android.content.DialogInterface.OnClickListener() {
					public void onClick(android.content.DialogInterface dialog, int which) {
						kamoflage_context.runOnUiThread(new Runnable() {
								public void run() {
									yesNoDialogCallback(
										false, dialog_identifier_var);
									kamoflage_context.removeDialog(YESNO_DIALOG);
								}
							});
					}
				}
				);
			return builder.create();
		}
		}
		return null;
	}

	static class AdditionalNativeThread extends java.lang.Thread {
		private int identity_hash;
		
		public AdditionalNativeThread(int _identity_hash) {
			identity_hash = _identity_hash;
		}

		public void run() {
			extraThreadEntry(identity_hash);
		}
	}

	public static void runAdditionalThread(int identity_hash) {
		AdditionalNativeThread ant = new AdditionalNativeThread(identity_hash);

		ant.start();
	}
	
	static class KamoflageMainNativeThread extends java.lang.Thread {
		KamoflageMainNativeThread() {
			super("MainKamoflageNativeThread");
		}

		public void run() {
			runMainNativeThread();
		}
	}
	public static KamoflageMainNativeThread __nativeMain;

	public View parse(InputStream file, ProgressBar mProgress, int current_progress) {
		View returnval = null;
		
		try {			
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			DocumentBuilder builder = factory.newDocumentBuilder();
			Document dom = builder.parse(file);
			Element root = dom.getDocumentElement();
			if(root.getTagName().equals("yapplication")) {
				returnval = KamoflageFactory.parse_yapplication(
					kamoflage_context,
					root, mProgress, current_progress);
				}		       
		} catch (javax.xml.parsers.ParserConfigurationException e) {
			android.widget.TextView tv = new android.widget.TextView(kamoflage_context);
			tv.setText(e.toString());
			returnval = tv;
		} catch (org.xml.sax.SAXException e) {
 			android.widget.TextView tv = new android.widget.TextView(kamoflage_context);
			tv.setText(e.toString());
			returnval = tv;
		} catch (java.io.IOException e) {
 			android.widget.TextView tv = new android.widget.TextView(kamoflage_context);
			tv.setText(e.toString());
			returnval = tv;
		}
		
		if(returnval == null) {
			android.widget.TextView tv = new android.widget.TextView(kamoflage_context);
			tv.setText("GUI was empty.");
			returnval = tv;
		}
		return returnval;
	}

	public native static void yesNoDialogCallback(boolean yes, int ptr_stored_in_int);
	public native static void onCancelableBusyWorkCallback();
	public native static void onUiThreadCallback();
	public native static void extraThreadEntry(int identity_hash);
	public native static void setDisplayConfiguration(
		float widthInInches, float heightInInches,
		float horizontalResolution, float verticalResolution,
		int edgeSlop);
	
	public native static void canvasEvent(long id, int evt, int x, int y);

	public native static void handleOnSurface(String id, int evt, int x, int y);
	
	public native static void handleOnModify(String id);
	public native static void handleOnValueChanged(String id);
	public native static void handleOnSelectRow(String id, int row_id);
	public native static void handleOnClick(String id);
	public native static void handleOnDoubleClick(String id);

	public native static boolean handleOnSelectNode(String id, int identifier);
	public native static boolean handleOnDeleteNode(String id, int identifier);
	public native static boolean handleOnDisconnect(String id, int output_id, int input_id,
							String output_socket, String input_socket);
	public native static boolean handleOnConnect(String id, int output_id, int input_id,
						     String output_socket, String input_socket);
	public native static void handleOnPoll(String id);

	public native static void tabViewChanged(String tab_id, String view_id);

	public native static long addCheckButton(String id, Object wid);
	public native static long addLabel(String id, Object wid);
	public native static long addButton(String id, Object wid);
	public native static long addCanvas(String id, Object wid);
	public native static long addSVGCanvas(String id, Object wid);
	public native static long addSurface(String id, Object wid);
	public native static long addList(String id, Object wid);
	public native static long addEntry(String id, Object wid);
	public native static long addScale(String id, Object wid);
	public native static long addContainer(String id, Object wid);
	public native static long addTabs(String id, Object wid);
	public native static long addWindow(String id, Object wid);
	public native static long addPollEvent(String id, Object wid);
	public native static long addUserEvent(String id, Object wid);
	
	public native static void runMainNativeThread();

}

	class KamoflageFactory {
		private static Kamoflage.Widget parse_pollevent(Element pe) {
			Kamoflage.PollEvent pev = new Kamoflage.PollEvent();
			pev.parse_widget(pe);
			pev.parse_poll_event(pe);
			pev.setNativeID(Kamoflage.addPollEvent(pev.get_id(), pev));
			return null;
		}
		
		private static Kamoflage.Widget parse_userevent(Element ue) {
			Kamoflage.UserEvent uev = new Kamoflage.UserEvent();
			uev.parse_widget(ue);
			uev.setNativeID(Kamoflage.addUserEvent(
						uev.get_id(), uev));
			return null;
		}
		
		private static Kamoflage.Widget parse_label(Element label) {
			Kamoflage.Label lbl = new Kamoflage.Label();
			lbl.parse_widget(label);
			lbl.parse_label(label);
			lbl.setNativeID(Kamoflage.addLabel(lbl.get_id(), lbl));
			return lbl;
		}
		
		private static Kamoflage.Widget parse_button(Element button) {
			Kamoflage.Button btn = new Kamoflage.Button();
			btn.parse_widget(button);
			btn.parse_button(button);
			btn.setNativeID(Kamoflage.addButton(btn.get_id(), btn));
			return btn;
		}

		private static Kamoflage.Widget parse_canvas(Element canvas) {
			Kamoflage.KamoCanvas cnv = new Kamoflage.KamoCanvas();
			cnv.parse_widget(canvas);
			cnv.setNativeID(Kamoflage.addCanvas(cnv.get_id(), cnv));
			cnv.parse_canvas(canvas);
			return cnv;
		}
		
		private static Kamoflage.Widget parse_svg_canvas(Element canvas) {
			Kamoflage.SVGCanvas cnv = new Kamoflage.SVGCanvas();
			cnv.parse_widget(canvas);
			cnv.setNativeID(Kamoflage.addSVGCanvas(cnv.get_id(), cnv));
			cnv.parse_svg_canvas(canvas);
			return cnv;
		}
		
		private static Kamoflage.Widget parse_surface(Element surface) {
			Kamoflage.Surface srf = new Kamoflage.Surface();
			srf.parse_widget(surface);
			srf.parse_surface(surface);
			srf.setNativeID(Kamoflage.addSurface(srf.get_id(), srf));
			return srf;
		}
		
		private static Kamoflage.Widget parse_list(Element list) {
			Kamoflage.List lst = new Kamoflage.List();
			lst.parse_widget(list);
			lst.parse_list(list);
			lst.setNativeID(Kamoflage.addList(lst.get_id(), lst));
			return lst;
		}
		
		private static Kamoflage.Widget parse_entry(Element entry) {
			Kamoflage.Entry ent = new Kamoflage.Entry();
			ent.parse_widget(entry);
			ent.setNativeID(Kamoflage.addEntry(ent.get_id(), ent));
			return ent;
		}
		
		private static Kamoflage.Widget parse_checkbutton(Element xbutton) {
			Kamoflage.CheckButton xbtn = new Kamoflage.CheckButton();
			xbtn.parse_widget(xbutton);
			xbtn.parse_checkbutton(xbutton);
			xbtn.setNativeID(Kamoflage.addCheckButton(xbtn.get_id(), xbtn));
			return xbtn;
		}

		private static Kamoflage.Widget parse_scale(Element scale) {
			Kamoflage.Scale scl = new Kamoflage.Scale();
			scl.parse_widget(scale);
			scl.parse_scale(scale);
			scl.setNativeID(Kamoflage.addScale(scl.get_id(), scl));
			return scl;
		}
		
		private static Kamoflage.Widget parse_container(Element container) {
			String align, fullscreen_str;

			align = container.getAttribute("align");

			Kamoflage.Container con = new Kamoflage.Container(container.getAttribute("id"), align);

			// parse additional widget attributes
			con.parse_widget(container);

			// Parse container specifics
			con.parse_container(container);
			
			// parse any widget nodes inside the window
			parse_node(con, container);

			con.setNativeID(Kamoflage.addContainer(con.get_id(), con));
			return con;
		}
		
		private static Kamoflage.Widget parse_tabs(Element tabs) {
			String tabs_on_top;			
			tabs_on_top = tabs.getAttribute("tabsontop");
			String hidetabs;			
			hidetabs = tabs.getAttribute("hidetabs");
			
			Kamoflage.Tabs tbs = new Kamoflage.Tabs(tabs.getAttribute("id"),
								tabs_on_top,
								hidetabs);

			// parse additional widget attributes
			tbs.parse_widget(tabs);

			tbs.setNativeID(Kamoflage.addTabs(tbs.get_id(), tbs));

			// parse any widget nodes inside the window
			parse_node(tbs, tabs);

			return tbs;
		}
		
		private static Kamoflage.Widget parse_window(Element window) {
			String align, fullscreen_str;

			align = window.getAttribute("align");
			fullscreen_str = window.getAttribute("fullscreen");
			boolean fullscreen = fullscreen_str.equals("true") ? true : false;

			Kamoflage.Window win = new Kamoflage.Window(align, fullscreen);

			// parse additional widget attributes
			win.parse_widget(window);

			// Parse container specifics
			win.parse_container(window);
			
			// parse any widget nodes inside the window
			parse_node(win, window);

			win.setNativeID(Kamoflage.addWindow(win.get_id(), win));
			return win;
		}
		
		private static void parse_node(Kamoflage.ContainerBase parent, Element node) {
			NodeList nodes = node.getChildNodes();

			for (int i = 0; i < nodes.getLength(); i++) {
				Node n = nodes.item(i);

				Kamoflage.Widget w = null;

				if(n.getNodeName().equals("checkbutton")) {
					w = parse_checkbutton((Element)n);
				}
				if(n.getNodeName().equals("label")) {
					w = parse_label((Element)n);
				}
				if(n.getNodeName().equals("button")) {
					w = parse_button((Element)n);
				}
				if(n.getNodeName().equals("canvas")) {
					w = parse_canvas((Element)n);
				}
				if(n.getNodeName().equals("svgcanvas")) {
					w = parse_svg_canvas((Element)n);
				}
				if(n.getNodeName().equals("surface")) {
					w = parse_surface((Element)n);
				}
				if(n.getNodeName().equals("list")) {
					w = parse_list((Element)n);
				}
				if(n.getNodeName().equals("entry")) {
					w = parse_entry((Element)n);
				}
				if(n.getNodeName().equals("scale")) {
					w = parse_scale((Element)n);
				}
				if(n.getNodeName().equals("container")) {
					w = parse_container((Element)n);
				}
				if(n.getNodeName().equals("tabs")) {
					w = parse_tabs((Element)n);
				}
				
				if(w != null)
					parent.add(w);
				
			}
		}

		static int current_progress = 0;
		static ProgressBar mProgress = null;
		static android.app.Activity kamoflage_context;
	
		public static View parse_yapplication(
			android.app.Activity kaco,
			Element yapp,
			ProgressBar _mProgress, int _current_progress) {
			View retval = null;
			kamoflage_context = kaco;
			NodeList nodes = yapp.getChildNodes();

			mProgress = _mProgress;
			current_progress = _current_progress;
			int progress_per_node = (100 - current_progress) / 
				nodes.getLength();

			if(progress_per_node <= 0) progress_per_node = 1;

			for(int i = 0; i < nodes.getLength(); i++) {
				Node n = nodes.item(i);

				Kamoflage.Widget w = null;

				if(n.getNodeName().equals("pollevent")) {
					w = parse_pollevent((Element)n);
				}
				if(n.getNodeName().equals("userevent")) {
					w = parse_userevent((Element)n);
				}
				if(n.getNodeName().equals("window")) {
					w = parse_window((Element)n);
					retval = w.internal;
				}

				if(current_progress < 100) {
					current_progress += progress_per_node;
					if(current_progress > 100)
						current_progress = 100;
				}
				
				kamoflage_context.runOnUiThread(new Runnable() {
						public void run() {
							mProgress.setProgress(current_progress);
						}
					});
			}

			Log.v("Kamoflage", "Before runMainNativeThread");
			Kamoflage.runMainNativeThread();
			Log.v("Kamoflage", "After runMainNativeThread");
			/*
			Kamoflage.__nativeMain = new Kamoflage.KamoflageMainNativeThread();
			Kamoflage.__nativeMain.start();
			*/
			return retval;
		}
	}
	
