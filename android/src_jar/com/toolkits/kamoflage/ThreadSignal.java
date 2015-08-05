/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Bjorling
 * Copyright (C) 2010 by Anton Persson
 * Copyright (C) 2011 by Anton Persson
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

import android.util.Log;
import java.lang.Object;

public class ThreadSignal extends Object {
	public class MonitorObject{
	}

	MonitorObject m = new MonitorObject();
	
	boolean wasSignalled = false;

	public void doWait(){
		synchronized(m){
			if(!wasSignalled){
				try{
					m.wait();
				} catch(InterruptedException e) {
					/* ignore */
				}
			}
			//clear signal and continue running.
			wasSignalled = false;
		}
	}
	
	public void doSignal(){
		synchronized(m){
			wasSignalled = true;
			m.notify();
		}
	}
}
	
