/**
 * Copyright 2005 New Roads School
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * \class nrsTable
 * This describes the nrsTable, which is a table created in JavaScript that is
 * able to be sorted and displayed in different ways based on teh configuration
 * parameters passed to it. 
 * to create a new table one only needs to call the setup function like so:
 * <pre>
 * nrsTable.setup(
 * {
 * 	table_name: "table_container",
 * 	table_data: d,
 * 	table_header: h
 * }
 * );
 * </pre>
 * Where table_name is the name of the table to build.  THis must be defined in
 * your HTML by putting a table declaration, such as <table id=table_name>
 * </table>.  This will declare where your table will be shown.
 * All sorts of parameters can be customized here.  For details look at the
 * function setup.
 * \see setup
 */


/**
 * Debug function.  Set debug to tru to view messages.
 * \param msg Message to display in an alert.
 */
debug = false;
function DEBUG(msg)
{
	if(debug)
		alert(msg);
}

/**
 * There is a memory leak problem that I can't seem to fix.  I'm attching 
 * something that I found from Aaron Boodman, which will clean up all the
 * memory leaks in the page (this can be found at http://youngpup.net/2005/0221010713
 * ). This is a little clunky, but it will do till I track this problem.
 * Oh, and this problem only occurrs is IE.
 */
if (window.attachEvent) {
    var clearElementProps = [
		'data',
        'onmouseover',
        'onmouseout',
        'onmousedown',
        'onmouseup',
        'ondblclick',
        'onclick',
        'onselectstart',
        'oncontextmenu'
    ];

    window.attachEvent("onunload", function() {
        var el;
        for(var d = document.all.length;d--;){
            el = document.all[d];
            for(var c = clearElementProps.length;c--;){
                el[clearElementProps[c]] = null;
            }
        }
    });
}


/**
 * This is the constructor.
 * It only needs the name of the table.  This should never be called directly, 
 * instead use setup function.
 * \param table The name of the table to create.
 * \see setup
 */
function nrsTable(table)
{
	this.my_table = table;
	this.field_to_sort = 0;
	this.field_asc = true;
}

new nrsTable('');
/**
 * This function is responsible for setting up an nrsTable.  All the parameters
 * can be configured directly from this function.  The params array of this 
 * function is a class (or a associative array, depending on how you want to
 * look at it) with the following possible parameters:
 * 	- table_name: required.  The id of the table tag.
 *	- table_header: required.  An array containing the header names.
 * 	- table_data: optional.  A 2D array of strings containing the cell contents.
 *	- caption: optional.  A caption to include on the table.
 *	- row_links: optional.  An array with hyperlinks per row.  Must be a javascript function.
 *	- cell_links: optional.  A 2D array with links on every cell.  Must be a javascript function
 *	- up_icon: optional.  A path to the ascending sort arrow.
 *	- down_icon: optional.  A path to the descending sort arrow.
 *	- prev_icon: optional.  A path to the previous page icon in the navigation.
 *	- next_icon: optional.  A path to the next page icon in the navigation.
 *	- rew_icon: optional.  A path to the first page icon in the navigation.
 *	- fwd_icon: optional.  A path to the last page icon in the navigation.
 *	- rows_per_page: optional.  The number of rows per page to display at any one time.
 *	- display_nav: optional.  Displays navigation (prev, next, first, last)
 *	- foot_headers: optional.  Whether to display th eheaders at the foot of the table.
 *	- header_color: optional.  The color of the header cells.  Will default to whatever is defined in CSS.
 *	- even_cell_color: optional.  The color of the even data cells.  Will default to whatever is defined in CSS.
 *	- odd_cell_color: optional.  The color of the odd data cells.  Will default to whatever is defined in CSS.
 *	- footer_color: optional.  The color of the footer cells.  Will default to whatever is defined in CSS.
 *	- hover_color: optional.  The color tha a row should turn when the mouse is over it.
 *	- padding: optional.  Individual cell padding, in pixels.
 *	- natural_compare: optional.  Uses the natural compare algorithm (separate from this program) to sort.
 *	- disable_sorting: optional.  An array specifying the columns top disable sorting on (0 is the first column).
 *
 * \params params An array as described above.
 */
nrsTable.setup = function(params)
{
	//here we assign all the veriables that we are passed, or the defaults if 
	//they are not defined.
	//Note that the only requirements are a table name and a header.
	if(typeof params['table_name'] == "undefined")
	{
		alert("Error! You must supply a table name!");
		return null;
	}
	if(typeof params['table_header'] == "undefined")
	{
		alert("Error! You must supply a table header!");
		return null;
	}
	
	//check if the global array exists, else create it.
	if(typeof(nrsTables) == "undefined")
	{
		eval("nrsTables = new Array();");
	}
	nrsTables[params['table_name']] = new nrsTable(params['table_name']);
	nrsTables[params['table_name']].heading = params['table_header'].concat();
	
	//now the non-required elements.  Data elements first
	nrsTables[params['table_name']].data = (typeof params['table_data'] == "undefined" || !params['table_data'])? null: params['table_data'].concat();
	nrsTables[params['table_name']].caption = (typeof params['caption'] == "undefined")? null: params['caption'];
	nrsTables[params['table_name']].row_links = (typeof params['row_links'] == "undefined" || !params['row_links'])? null: params['row_links'].concat();
	nrsTables[params['table_name']].cell_links = (typeof params['cell_links'] == "undefined" || !params['row_links'])? null: params['cell_links'].concat();
	
	//these are the icons.
	nrsTables[params['table_name']].up_icon = (typeof params['up_icon'] == "undefined")? "up.gif": params['up_icon'];
	nrsTables[params['table_name']].down_icon = (typeof params['down_icon'] == "undefined")? "down.gif": params['down_icon'];
	nrsTables[params['table_name']].prev_icon = (typeof params['prev_icon'] == "undefined")? "left.gif": params['prev_icon'];
	nrsTables[params['table_name']].next_icon = (typeof params['next_icon'] == "undefined")? "right.gif": params['next_icon'];
	nrsTables[params['table_name']].rew_icon = (typeof params['rew_icon'] == "undefined")? "first.gif": params['rew_icon'];
	nrsTables[params['table_name']].fwd_icon = (typeof params['fwd_icon'] == "undefined")? "last.gif": params['fwd_icon'];
	
	//now the look and feel options.
	nrsTables[params['table_name']].rows_per_page = (typeof params['rows_per_page'] == "undefined")? -1: params['rows_per_page'];
	nrsTables[params['table_name']].page_nav = (typeof params['page_nav'] == "undefined")? false: params['page_nav'];
	nrsTables[params['table_name']].foot_headers = (typeof params['foot_headers'] == "undefined")? false: params['foot_headers'];
	nrsTables[params['table_name']].header_color = (typeof params['header_color'] == "undefined")? null: params['header_color'];
	nrsTables[params['table_name']].even_cell_color = (typeof params['even_cell_color'] == "undefined")? null: params['even_cell_color'];
	nrsTables[params['table_name']].odd_cell_color = (typeof params['odd_cell_color'] == "undefined")? null: params['odd_cell_color'];
	nrsTables[params['table_name']].footer_color = (typeof params['footer_color'] == "undefined")? null: params['footer_color'];
	nrsTables[params['table_name']].hover_color = (typeof params['hover_color'] == "undefined")? null: params['hover_color'];
	nrsTables[params['table_name']].padding = (typeof params['padding'] == "undefined")? null: params['padding'];
	nrsTables[params['table_name']].natural_compare = (typeof params['natural_compare'] == "undefined")? false: true;
	nrsTables[params['table_name']].disable_sorting = 
		(typeof params['disable_sorting'] == "undefined")? false: "." + params['disable_sorting'].join(".") + ".";
	//finally, build the table
	nrsTables[params['table_name']].buildTable();
};


/**
 * This is the Javascript quicksort implementation.  This will sort the 
 * this.data and the this.data_nodes based on the this.field_to_sort parameter.
 * \param left The left index of the array.
 * \param right The right index of the array
 */
nrsTable.prototype.quickSort = function(left, right)
{
	if(!this.data || this.data.length == 0)
		return;
//	alert("left = " + left + " right = " + right);
	var i = left;
	var j = right;
	var k = this.data[Math.round((left + right) / 2)][this.field_to_sort];
	if (k != '') {
           if (isNaN(k)) {
             k = k.toLowerCase();
           } else {
	     k = parseInt(k, 10);
	   }
        }

	while(j > i)
	{
		if(this.field_asc)
		{
			while(this.data[i][this.field_to_sort].toLowerCase() < k)
				i++;
			while(this.data[j][this.field_to_sort].toLowerCase() > k)
				j--;
		}
		else
		{
			while(this.data[i][this.field_to_sort].toLowerCase() > k)
				i++;
			while(this.data[j][this.field_to_sort].toLowerCase() < k)
				j--;
		}
		if(i <= j )
		{
			//swap both values
			//sort data
			var temp = this.data[i];
			this.data[i] = this.data[j];
			this.data[j] = temp;
			
			//sort contents
			var temp = this.data_nodes[i];
			this.data_nodes[i] = this.data_nodes[j];
			this.data_nodes[j] = temp;
			i++;
			j--;
		}
	}
	if(left < j)
		this.quickSort(left, j);
	if(right > i)
		this.quickSort(i, right);
}

/**
 * This is the Javascript natural sort function.  Because of some obscure JavaScript
 * quirck, we could not do quicsort while calling natcompare to compare, so this
 * function will so a simple bubble sort using the natural compare algorithm.
 */
nrsTable.prototype.natSort = function()
{
	if(!this.data || this.data.length == 0)
		return;
	var swap;
	for(i = 0; i < this.data.length - 1; i++)
	{
		for(j = i; j < this.data.length; j++)
		{
			if(!this.field_asc)
			{
				if(natcompare(this.data[i][this.field_to_sort].toLowerCase(), 
					this.data[j][this.field_to_sort].toLowerCase()) == -1)
					swap = true;
				else
					swap = false;
			}
			else
			{
				if(natcompare(this.data[i][this.field_to_sort].toLowerCase(), 
					this.data[j][this.field_to_sort].toLowerCase()) == 1)
					swap = true;
				else
					swap = false;
			}
			if(swap)
			{
				//swap both values
				//sort data
				var temp = this.data[i];
				this.data[i] = this.data[j];
				this.data[j] = temp;
				
				//sort contents
				var temp = this.data_nodes[i];
				this.data_nodes[i] = this.data_nodes[j];
				this.data_nodes[j] = temp;
			}
		}
	}
}

/**
 * This function will recolor all the the nodes to conform to the alternating 
 * row colors.
 */
nrsTable.prototype.recolorRows = function()
{
	if(this.even_cell_color || this.odd_cell_color)
	{
		DEBUG("Recoloring Rows. length = " + this.data_nodes.length);
		for(var i = 0; i < this.data_nodes.length; i++)
		{
			if(i % 2 == 0)
			{
				if(this.even_cell_color)
					this.data_nodes[i].style.backgroundColor = this.even_cell_color;
				this.data_nodes[i].setAttribute("id", "even_row");
			}
			else
			{
				if(this.odd_cell_color)
					this.data_nodes[i].style.backgroundColor = this.odd_cell_color;
				this.data_nodes[i].setAttribute("id", "odd_row");
			}
		}
	}
}

/**
 * This function will create the Data Nodes, which are a reference to the table
 * rows in the HTML.
 */
nrsTable.prototype.createDataNodes = function()
{
	if(this.data_nodes)
		delete this.data_nodes;
	this.data_nodes = new Array();
	if(!this.data)
		return;
	for(var i = 0; i < this.data.length; i++)
	{
		var curr_row = document.createElement("TR");
		
		for(var j = 0; j < this.data[i].length; j++)
		{
			var curr_cell = document.createElement("TD");
			//do we need to create links on every cell?
			if(this.cell_links)
			{
				var fn = new Function("", this.cell_links[i][j]);
				curr_cell.onclick = fn;
				curr_cell.style.cursor = 'pointer';
			}
			//workaround for IE
			curr_cell.setAttribute("className", "dataTD" + j);
			//assign the padding
			if(this.padding)
			{
				curr_cell.style.paddingLeft = this.padding + "px";
				curr_cell.style.paddingRight = this.padding + "px";
			}
			
			if (typeof this.data[i][j] == "object") {
				curr_cell.appendChild(this.data[i][j]);
			} else {
				curr_cell.appendChild(document.createTextNode(this.data[i][j]));
			}

			curr_row.appendChild(curr_cell);
		}
		//do we need to create links on every row?
		if(!this.cell_links && this.row_links)
		{
			var fn = new Function("", this.row_links[i]);
			curr_row.onclick = fn;
			curr_row.style.cursor = 'pointer';
		}
		//sets the id for odd and even rows.
		if(i % 2 == 0)
		{
			curr_row.setAttribute("id", "even_row");
			if(this.even_cell_color)
				curr_row.style.backgroundColor = this.even_cell_color;
		}
		else
		{
			curr_row.setAttribute("id", "odd_row");
			if(this.odd_cell_color)
				curr_row.style.backgroundColor = this.odd_cell_color;
		}
		if(this.hover_color)
		{
			curr_row.onmouseover = new Function("", "this.style.backgroundColor='" + this.hover_color + "';");
			curr_row.onmouseout = new Function("", "this.style.backgroundColor=(this.id=='even_row')?'" + 
								this.even_cell_color + "':'" + this.odd_cell_color + "';");
		}
		this.data_nodes[i] = curr_row;
	}
}

/**
 * This function will update the nav page display.
 */
nrsTable.prototype.updateNav = function()
{
	if(this.page_nav)
	{
		var p = 0;
		if(this.foot_headers)
			p++;
		var t = document.getElementById(this.my_table);
		var nav = t.tFoot.childNodes[p];
		if(nav)
		{
			var caption = t.tFoot.childNodes[p].childNodes[0].childNodes[2];
			caption.innerHTML = "Page " + (this.current_page + 1) + " of " + this.num_pages;
		}
		else
		{
			if(this.num_pages > 1)
			{
				this.insertNav();
				nav = t.tFoot.childNodes[p];
			}
		}
		if(nav)
		{
			if(this.current_page == 0)
				this.hideLeftArrows();
			else
				this.showLeftArrows();
			
			if(this.current_page + 1 == this.num_pages)
				this.hideRightArrows();
			else
				this.showRightArrows();
		}
	}
}

/**
 * This function will flip the sort arrow in place.  If a heading is used in the
 * footer, then it will flip that one too.
 */
nrsTable.prototype.flipSortArrow = function()
{
	this.field_asc = !this.field_asc;
	//flip the arrow on the heading.
	var heading = document.getElementById(this.my_table).tHead.childNodes[0].childNodes[this.field_to_sort];
	if(this.field_asc)
		heading.getElementsByTagName("IMG")[0].setAttribute("src", this.up_icon);
	else
		heading.getElementsByTagName("IMG")[0].setAttribute("src", this.down_icon);
	//is there a heading in the footer?
	if(this.foot_headers)
	{
		//yes, so flip that arrow too.
		var footer = document.getElementById(this.my_table).tFoot.childNodes[0].childNodes[this.field_to_sort];
		if(this.field_asc)
			footer.getElementsByTagName("IMG")[0].setAttribute("src", this.up_icon);
		else
			footer.getElementsByTagName("IMG")[0].setAttribute("src", this.down_icon);
	}
}

/**
 * This function will move the sorting arrow from the place specified in 
 * this.field_to_sort to the passed parameter.  It will also set 
 * this.field_to_sort to the new value.  It will also do it in the footers, 
 * if they exists.
 * \param field The new field to move it to.
 */
nrsTable.prototype.moveSortArrow = function(field)
{
	var heading = document.getElementById(this.my_table).tHead.childNodes[0].childNodes[this.field_to_sort];
	var img = heading.removeChild(heading.getElementsByTagName("IMG")[0]);
	heading = document.getElementById(this.my_table).tHead.childNodes[0].childNodes[field];
	heading.appendChild(img);
	//are there headers in the footers.
	if(this.foot_headers)
	{
		//yes, so switch them too.
		var footer = document.getElementById(this.my_table).tFoot.childNodes[0].childNodes[this.field_to_sort];
		var img = footer.removeChild(footer.getElementsByTagName("IMG")[0]);
		footer = document.getElementById(this.my_table).tFoot.childNodes[0].childNodes[field];
		footer.appendChild(img);
	}
	//finally, set the field to sort by.
	this.field_to_sort = field;
}

/**
 * This function completely destroys a table.  Should be used only when building
 * a brand new table (ie, new headers).  Else you should use a function like
 * buildNewData which only deletes the TBody section.
 */
nrsTable.prototype.emptyTable = function()
{
	var t = document.getElementById(this.my_table);
	while(t.childNodes.length != 0)
		t.removeChild(t.childNodes[0]);
};

/**
 * This function builds a brand new table from scratch.  This function should
 * only be called when a brand new table (with headers, footers, etc) needs
 * to be created.  NOT when refreshing data or changing data.
 */
nrsTable.prototype.buildTable = function()
{
	//reset the sorting information.
	this.field_to_sort = 0;
	this.field_asc = true;
	
	//remove the nodes links.
	delete this.data_nodes;
	
	//do we have to calculate the number of pages?
	if(this.data && this.rows_per_page != -1)
	{
		//we do.
		this.num_pages = Math.ceil(this.data.length / this.rows_per_page);
		this.current_page = 0;
	}
	
	//blank out the table.
	this.emptyTable();
	
	//this is the table that we will be using.
	var table = document.getElementById(this.my_table);
	
	//is there a caption?
	if(this.caption)
	{
		var caption = document.createElement("CAPTION");
		caption.setAttribute("align", "top");
		caption.appendChild(document.createTextNode(this.caption));
		table.appendChild(caption);
	}
	
	//do the heading first
	var table_header = document.createElement("THEAD");
	var table_heading = document.createElement("TR");
	//since this is a new table the first field is what's being sorted.
	var curr_cell = document.createElement("TH");
	var fn = new Function("", "nrsTables['" + this.my_table + "'].fieldSort(" + 0 + ");");
	if(!this.disable_sorting || this.disable_sorting.indexOf(".0.") == -1)
		curr_cell.onclick = fn;
	if(this.header_color)
		curr_cell.style.backgroundColor = this.header_color;
	curr_cell.style.cursor = 'pointer';
	var img = document.createElement("IMG");
	img.setAttribute("src", this.up_icon);
	img.setAttribute("border", "0");
	img.setAttribute("height", "8");
	img.setAttribute("width", "8");
	curr_cell.appendChild(document.createTextNode(this.heading[0]));
	curr_cell.appendChild(img);
	table_heading.appendChild(curr_cell);
	//now do the rest of the heading.
	for(var i = 1; i < this.heading.length; i++)
	{
		curr_cell = document.createElement("TH");
		var fn = new Function("", "nrsTables['" + this.my_table + "'].fieldSort(" + i + ");");
		if(!this.disable_sorting || this.disable_sorting.indexOf("." + i + ".") == -1)
			curr_cell.onclick = fn;
		if(this.header_color)
			curr_cell.style.backgroundColor = this.header_color;
		curr_cell.style.cursor = 'pointer';
		//build the sorter
		curr_cell.appendChild(document.createTextNode(this.heading[i]));
		table_heading.appendChild(curr_cell);
	}
	table_header.appendChild(table_heading);
	
	//now the content
	var table_body = document.createElement("TBODY");
	this.createDataNodes();
	if(this.data)
	{
		if(this.natural_compare)
			this.natSort(0, this.data.length - 1);
		else
			this.quickSort(0, this.data.length - 1);
		this.recolorRows();
	}

	//finally, the footer
	var table_footer = document.createElement("TFOOT");
	if(this.foot_headers)
	{
		table_footer.appendChild(table_heading.cloneNode(true));
	}
	
	if(this.page_nav && this.num_pages > 1)
	{
		//print out the page navigation
		//first and previous page
		var nav = document.createElement("TR");
		var nav_cell = document.createElement("TH");
		nav_cell.colSpan = this.heading.length;
		
		var left = document.createElement("DIV");
		if(document.attachEvent)
			left.style.styleFloat = "left";
		else
			left.style.cssFloat = "left";
		var img = document.createElement("IMG");
		img.setAttribute("src", this.rew_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].firstPage();");
		img.style.cursor = 'pointer';
		left.appendChild(img);
		//hack to space the arrows, cause IE is absolute crap
		left.appendChild(document.createTextNode(" "));
		img = document.createElement("IMG");
		img.setAttribute("src", this.prev_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].prevPage();");
		img.style.cursor = 'pointer';
		left.appendChild(img);
		//apend it to the cell
		nav_cell.appendChild(left);
		
		//next and last pages
		var right = document.createElement("DIV");
		if(document.attachEvent)
			right.style.styleFloat = "right";
		else
			right.style.cssFloat = "right";
		img = document.createElement("IMG");
		img.setAttribute("src", this.next_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].nextPage();");
		img.style.cursor = 'pointer';
		right.appendChild(img);
		//hack to space the arrows, cause IE is absolute crap
		right.appendChild(document.createTextNode(" "));
		img = document.createElement("IMG");
		img.setAttribute("src", this.fwd_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "JavaScript:nrsTables['" + this.my_table + "'].lastPage();");
		img.style.cursor = 'pointer';
		right.appendChild(img);
		//apend it to the cell
		nav_cell.appendChild(right);
		
		//page position
		var pos = document.createElement("SPAN");
		pos.setAttribute("id", "nav_pos");
		pos.appendChild(document.createTextNode("Page " + 
						(this.current_page + 1) + " of " + this.num_pages));
		//append it to the cell.
		nav_cell.appendChild(pos);
		
		nav.appendChild(nav_cell);
		//append it to the footer
		table_footer.appendChild(nav);
	}
	
	if(this.footer_color)
	{
		for(var i = 0; i < table_footer.childNodes.length; i++)
			table_footer.childNodes[i].style.backgroundColor = this.footer_color;
	}
	
	//append the data
	table.appendChild(table_header);
	table.appendChild(table_body);
	table.appendChild(table_footer);
	if(this.data)
	{
		if(this.natural_compare)
			this.natSort(0, this.data.length - 1);
		else
			this.quickSort(0, this.data.length - 1);
	}
	this.refreshTable();
};

/**
 * This function will remove the elements in teh TBody section of the table and
 * return an array of references of those elements.  This array can then be 
 * sorted and re-inserted into the table.
 * \return An array to references of the TBody contents.
 */
nrsTable.prototype.extractElements = function()
{
	var tbody = document.getElementById(this.my_table).tBodies[0];
	var nodes = new Array();
	var i = 0;
	while(tbody.childNodes.length > 0)
	{
		nodes[i] = tbody.removeChild(tbody.childNodes[0]);
		i++;
	}
	return nodes;
}

/**
 * This function will re-insert an array of elements into the TBody of a table.
 * Note that the array elements are stored in the this.data_nodes reference.
 */
nrsTable.prototype.insertElements = function()
{
	var tbody = document.getElementById(this.my_table).tBodies[0];
	var start = 0;
	var num_elements = this.data_nodes.length;
	if(this.rows_per_page != -1)
	{
		start = this.current_page * this.rows_per_page;
		num_elements = (this.data_nodes.length - start) > this.rows_per_page?
							this.rows_per_page + start:
							this.data_nodes.length;
	}
	DEBUG("start is " + start + " and num_elements is " + num_elements);
	for(var i = start; i < num_elements; i++)
	{
		tbody.appendChild(this.data_nodes[i]);
	}
}

/**
 * This function will sort the table's data by a specific field.  The field 
 * parameter referes to which field index should be sorted.
 * \param field The field index which to sort on.
 */
nrsTable.prototype.fieldSort = function(field)
{
	if(this.field_to_sort == field)
	{
		//only need to reverse the array.
		if(this.data)
		{
			this.data.reverse();
			this.data_nodes.reverse();
		}
		//flip the arrow on the heading.
		this.flipSortArrow();
	}
	else
	{
		//In this case, we need to sort the array.  We'll sort it last, first 
		//make sure that the arrow images are set correctly.
		this.moveSortArrow(field);
		//finally, set the field to sort by.
		this.field_to_sort = field;
		if(this.data)
		{
			//we'll be using our implementation of quicksort
			if(this.natural_compare)
				this.natSort(0, this.data.length - 1);
			else
				this.quickSort(0, this.data.length - 1);
		}
	}
	//finally, we refresh the table.
	this.refreshTable();
};

/**
 * This function will refresh the data in the table.  This function should be
 * used whenever the nodes have changed, or when chanign pages.  Note that 
 * this will NOT re-sort.
 */
nrsTable.prototype.refreshTable = function()
{
	this.extractElements();
	this.recolorRows();
	this.insertElements();
	//finally, if there is a nav, upate it.
	this.updateNav();
}

/**
 * This function will advance a page.  If we are already at the last page, then 
 * it will remain there.
 */
nrsTable.prototype.nextPage = function()
{
	DEBUG("current page is " + this.current_page + " and num_pages is " + this.num_pages);
	if(this.current_page + 1 != this.num_pages)
	{
		this.current_page++;
		this.refreshTable();
	}
	DEBUG("current page is " + this.current_page + " and num_pages is " + this.num_pages);
}

/**
 * This function will go back a page.  If we are already at the first page, then 
 * it will remain there.
 */
nrsTable.prototype.prevPage = function()
{
	DEBUG("current page is " + this.current_page + " and num_pages is " + this.num_pages);
	if(this.current_page != 0)
	{
		this.current_page--;
		this.refreshTable();
	}
	DEBUG("current page is " + this.current_page + " and num_pages is " + this.num_pages);
}

/**
 * This function will go to the first page.
 */
nrsTable.prototype.firstPage = function()
{
	if(this.current_page != 0)
	{
		this.current_page = 0;
		this.refreshTable();
	}
}

/**
 * This function will go to the last page.
 */
nrsTable.prototype.lastPage = function()
{
	DEBUG("lastPage(), current_page: " + this.current_page + " and num_pages: " + this.num_pages);
	if(this.current_page != (this.num_pages - 1))
	{
		this.current_page = this.num_pages - 1;
		this.refreshTable();
	}
}

/**
 * This function will go to a specific page.  valid values are pages 1 to 
 * however many number of pages there are.
 * \param page The page number to go to.
 */
nrsTable.prototype.gotoPage = function(page)
{
	page--;
	if(page >=0 && page < this.num_pages)
	{
		this.current_page = page;
		this.refreshTable();
	}
}

/**
 * This function can be used to change the number of entries per row displayed
 * on the fly.
 * \param entries The number of entries per page.
 */
nrsTable.prototype.changeNumRows = function(entries)
{
	if(entries > 0)
	{
		this.rows_per_page = entries;
		//we do.
		this.num_pages = Math.ceil(this.data.length / this.rows_per_page);
		this.refreshTable();
	}
}

/**
 * This function will take in a new data array and , optionally, a new cell_link
 * array OR a new row_link array.  Only one will be used, with the cell_link
 * array taking precedence.  It will then re-build the table with the new data
 * array.
 * \param new_data This is the new data array.  This is required.
 * \param cell_links This is the new cell links array, a 2D array for each cell.
 * \param row_links This is the new row links array, a 1D array for each row.
 */
nrsTable.prototype.newData = function(new_data, cell_links, row_links)
{
	//extract the elements from teh table to clear the table.
	this.extractElements();
	//now delete all the data related to this table.  I do this so that 
	//(hopefully) the memory will be freed.  This is realy needed for IE, whose
	//memory handling is almost non-existant
	delete this.data;
	delete this.data_nodes;
	delete this.cell_links;
	delete this.row_links
	//now re-assign.
	this.data = new_data;
	this.cell_links = cell_links;
	this.row_links = row_links;
	if(this.rows_per_page != -1)
	{
		//we do.
		this.num_pages = Math.ceil(this.data.length / this.rows_per_page);
		if(this.num_pages <= 1 && this.page_nav)
			this.removeNav();
		else if(this.page_nav)
			this.insertNav();
		this.current_page = 0;
	}
	this.createDataNodes();
	if(this.field_to_sort != 0)
		this.moveSortArrow(0);
	if(!this.field_asc)
		this.flipSortArrow();
	this.insertElements();
	this.updateNav();
}

/**
 * This function will remove the NAV bar (if one exists) from the table.
 */
nrsTable.prototype.removeNav = function()
{
	if(this.page_nav)
	{
		//in this case, remove the nav from the existing structure.
		var table = document.getElementById(this.my_table);
		var p = 0;
		if(this.foot_headers)
			p++;
		var nav = table.tFoot.childNodes[p];
		if(nav)
		{
			table.tFoot.removeChild(nav);
			delete nav;
		}
	}
}

/**
 * This function wil re-insert the nav into the table.
 */
nrsTable.prototype.insertNav = function()
{
	table = document.getElementById(this.my_table);
	var p = 0;
	if(this.foot_headers)
		p++;
	if(this.page_nav && !table.tFoot.childNodes[p])
	{
		//this means there should be a nav and there isn't one.
		//print out the page navigation
		//first and previous page
		var nav = document.createElement("TR");
		var nav_cell = document.createElement("TH");
		nav_cell.colSpan = this.heading.length;
		
		var left = document.createElement("DIV");
		if(document.attachEvent)
			left.style.styleFloat = "left";
		else
			left.style.cssFloat = "left";
		var img = document.createElement("IMG");
		img.setAttribute("src", this.rew_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].firstPage();");
		img.style.cursor = 'pointer';
		left.appendChild(img);
		//hack to space the arrows, cause IE is absolute crap
		left.appendChild(document.createTextNode(" "));
		img = document.createElement("IMG");
		img.setAttribute("src", this.prev_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].prevPage();");
		img.style.cursor = 'pointer';
		left.appendChild(img);
		//apend it to the cell
		nav_cell.appendChild(left);
		
		//next and last pages
		var right = document.createElement("DIV");
		if(document.attachEvent)
			right.style.styleFloat = "right";
		else
			right.style.cssFloat = "right";
		img = document.createElement("IMG");
		img.setAttribute("src", this.next_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "nrsTables['" + this.my_table + "'].nextPage();");
		img.style.cursor = 'pointer';
		right.appendChild(img);
		//hack to space the arrows, cause IE is absolute crap
		right.appendChild(document.createTextNode(" "));
		img = document.createElement("IMG");
		img.setAttribute("src", this.fwd_icon);
		img.setAttribute("border", "0");
		img.setAttribute("height", "10");
		img.setAttribute("width", "10");
		img.onclick = new Function("", "JavaScript:nrsTables['" + this.my_table + "'].lastPage();");
		img.style.cursor = 'pointer';
		right.appendChild(img);
		//apend it to the cell
		nav_cell.appendChild(right);
		
		//page position
		var pos = document.createElement("SPAN");
		pos.setAttribute("id", "nav_pos");
		pos.appendChild(document.createTextNode("Page " + 
						(this.current_page + 1) + " of " + this.num_pages));
		//append it to the cell.
		nav_cell.appendChild(pos);
		
		nav.appendChild(nav_cell);
		//append it to the footer
		table.tFoot.appendChild(nav);
	}
}

/**
 * This function will hide the previous arrow and the rewind arrows from the
 * nav field.
 */
nrsTable.prototype.hideLeftArrows = function()
{
	if(!this.page_nav)
		return;
	var myTable = document.getElementById(this.my_table);
	var p = 0;
	if(this.foot_headers)
		p++;
	var nav = myTable.tFoot.childNodes[p];
	nav.childNodes[0].childNodes[0].style.display = "none";
}

/**
 * This function will show the previous arrow and the rewind arrows from the
 * nav field.
 */
nrsTable.prototype.showLeftArrows = function()
{
	if(!this.page_nav)
		return;
	table = document.getElementById(this.my_table);
	var p = 0;
	if(this.foot_headers)
		p++;
	var nav = table.tFoot.childNodes[p];
	nav.childNodes[0].childNodes[0].style.display = "block";
}

/**
 * This function will hide the next arrow and the fast foward arrows from the
 * nav field.
 */
nrsTable.prototype.hideRightArrows = function()
{
	if(!this.page_nav)
		return;
	table = document.getElementById(this.my_table);
	var p = 0;
	if(this.foot_headers)
		p++;
	var nav = table.tFoot.childNodes[p];
	nav.childNodes[0].childNodes[1].style.display = "none";
}

/**
 * This function will show the next arrow and the fast foward arrows from the
 * nav field.
 */
nrsTable.prototype.showRightArrows = function()
{
	if(!this.page_nav)
		return;
	table = document.getElementById(this.my_table);
	var p = 0;
	if(this.foot_headers)
		p++;
	var nav = table.tFoot.childNodes[p];
	nav.childNodes[0].childNodes[1].style.display = "block";
}
