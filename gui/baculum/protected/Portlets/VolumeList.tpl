<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<%=(isset($this->DataItem->pool->name) && $this->getPage()->VolumeWindow->oldPool != $this->DataItem->pool->name) ? '<div class="window-section"><span>' . Prado::localize('Pool:') . ' ' . $this->DataItem->pool->name  . '<span></div>': ''%>
			<com:TPanel ID="VolumeElement" CssClass="slide-window-element" ToolTip="<%=(isset($this->DataItem->recycle) && $this->DataItem->recycle == 1 && !empty($this->DataItem->lastwritten) && in_array($this->DataItem->volstatus, array('Full', 'Used'))) ? Prado::localize('When expire:') . date( 'Y-m-d H:i:s', (strtotime($this->DataItem->lastwritten) + $this->DataItem->volretention)) : ''%> <%=Prado::localize('Last written:')%> <%=!empty($this->DataItem->lastwritten) ? $this->DataItem->lastwritten : Prado::localize('never written')%>">
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/media-icon.png" alt="" /><%=@$this->DataItem->volumename%>
				<div id="<%=isset($this->DataItem->volumename) ? $this->DataItem->volumename : ''%>_sizebar" class="status-bar-<%=isset($this->DataItem->volstatus) ? strtolower($this->DataItem->volstatus) : ''%>"><%=isset($this->DataItem->volstatus) ? $this->DataItem->volstatus : ''%></div>
				<input type="hidden" name="<%=$this->ClientID%>" value="<%=isset($this->DataItem->mediaid) ? $this->DataItem->mediaid : ''%>" />
			</com:TPanel>
			<%=!(isset($this->DataItem->pool->name) ? ($this->getPage()->VolumeWindow->oldPool = $this->DataItem->pool->name) : false)%>
		</prop:ItemTemplate>
	</com:TActiveRepeater>
	</com:TActivePanel>
	<com:TActivePanel ID="DataGridShow">
	<com:TActiveDataGrid
		ID="DataGrid"
		AutoGenerateColumns="false"
		AllowSorting="false"
		OnSortCommand="sortDataGrid"
		CellPadding="5px"
		CssClass="window-section-detail"
		ItemStyle.CssClass="slide-window-element"
		AlternatingItemStyle.CssClass="slide-window-element-alternating"
	>
		<com:TActiveTemplateColumn HeaderText="<input type='checkbox' name='actions_checkbox' onclick=SlideWindow.getObj('VolumeWindow').markAllChecked(this.checked)>" ItemStyle.HorizontalAlign="Center">
			<prop:ItemTemplate>
				<input type="checkbox" name="actions_checkbox" value="<%=$this->getParent()->Data['volumename']%>" id="<%=$this->getPage()->VolumeWindow->CheckedValues->ClientID%><%=$this->getParent()->Data['volumename']%>" rel="<%=$this->getPage()->VolumeWindow->CheckedValues->ClientID%>" onclick="SlideWindow.getObj('VolumeWindow').markChecked(this.getAttribute('rel'), this.checked, this.value, true);" />
			</prop:ItemTemplate>
                </com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn HeaderText="<%[ Volume name ]%>" SortExpression="volumename">
			<prop:ItemTemplate>
				<div><%=$this->getParent()->Data['volumename']%></div>
				<input type="hidden" name="<%=$this->getParent()->ClientID%>" value="<%=$this->getParent()->Data['mediaid']%>" />
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="slot"
			HeaderText="<%[ Slot ]%>"
			DataField="slot"
			ItemStyle.HorizontalAlign="Center"
		/>
		<com:TActiveTemplateColumn HeaderText="<%[ Pool ]%>" SortExpression="pool">
			<prop:ItemTemplate>
				<%=$this->getParent()->Data['pool']['name']%>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn HeaderText="<%[ Status ]%>" SortExpression="volstatus">
			<prop:ItemTemplate>
				<div id="<%=$this->getParent()->Data['volumename']%>_sizebar" class="status-bar-detail-<%=strtolower($this->getParent()->Data['volstatus'])%>"><%=$this->getParent()->Data['volstatus']%></div>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="whenexpire"
			HeaderText="<%[ When expire ]%>"
			DataField="whenexpire"
		/>
	</com:TActiveDataGrid>
	<com:TActiveHiddenField ID="CheckedValues" />
	</com:TActivePanel>
	<com:TCallback ID="DataElementCall" OnCallback="Page.VolumeWindow.configure">
		<prop:ClientSide.OnComplete>
			ConfigurationWindow.getObj('VolumeWindow').show();
			ConfigurationWindow.getObj('VolumeWindow').progress(false);
		</prop:ClientSide.OnComplete>
	</com:TCallback>
</com:TContent>
