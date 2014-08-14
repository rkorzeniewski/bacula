<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<script type="text/javascript">
		document.observe("dom:loaded", function() {
			volumeConfigurationWindow = ConfigurationWindow<%=$this->getPage()->VolumeConfiguration->getMaster()->ClientID%>;
			volumeSlideWindowObj = <%=$this->getPage()->VolumeWindow->ShowID%>SlideWindow;
		});
	</script>
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<%=(isset($this->DataItem->pool->name) && $this->getPage()->VolumeWindow->oldPool != $this->DataItem->pool->name) ? '<div class="window-section"><span>Pool: ' . $this->DataItem->pool->name  . '<span></div>': ''%>
			<com:TPanel ID="VolumeElement" CssClass="slide-window-element" ToolTip="<%=(isset($this->DataItem->recycle) && $this->DataItem->recycle == 1 && !empty($this->DataItem->lastwritten) && in_array($this->DataItem->volstatus, array('Full', 'Used'))) ? 'When expire: ' . date( 'Y-m-d H:i:s', (strtotime($this->DataItem->lastwritten) + $this->DataItem->volretention)) : ''%> Last written: <%=!empty($this->DataItem->lastwritten) ? $this->DataItem->lastwritten : 'never written'%>">
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/media-icon.png" alt="" /><%=@$this->DataItem->volumename%>
				<div id="<%=isset($this->DataItem->volumename) ? $this->DataItem->volumename : ''%>_sizebar" class="status-bar-<%=isset($this->DataItem->volstatus) ? strtolower($this->DataItem->volstatus) : ''%>"><%=isset($this->DataItem->volstatus) ? $this->DataItem->volstatus : ''%></div>
			</com:TPanel>
			<com:TCallback ID="VolumeElementCall" OnCallback="Page.VolumeWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem->mediaid%>">
				<prop:ClientSide.OnComplete>
					volumeConfigurationWindow.show();
					volumeConfigurationWindow.progress(false);
				</prop:ClientSide.OnComplete>
			</com:TCallback>
			<script type="text/javascript">
				$('<%=$this->VolumeElement->ClientID%>').observe('click', function() {
					var request = <%= $this->VolumeElementCall->ActiveControl->Javascript %>;
					volumeConfigurationWindow.openConfigurationWindow(request, volumeSlideWindowObj);
				});
			</script>
			<%=!(isset($this->DataItem->pool->name) ? ($this->getPage()->VolumeWindow->oldPool = $this->DataItem->pool->name) : false)%>
		</prop:ItemTemplate>
	</com:TActiveRepeater>
	</com:TActivePanel>
	<com:TActivePanel ID="DataGridShow">
	<com:TActiveDataGrid
		ID="DataGrid"
		AutoGenerateColumns="false"
		AllowSorting="true"
		OnSortCommand="sortDataGrid"
		CellPadding="5px"
		CssClass="window-section-detail"
		ItemStyle.CssClass="slide-window-element"
		AlternatingItemStyle.CssClass="slide-window-element-alternating"
	>
		<com:TActiveTemplateColumn HeaderText="Volume name" SortExpression="volumename">
			<prop:ItemTemplate>
				<com:TPanel ID="VolumeTableElement"><%=$this->getParent()->Data['volumename']%></com:TPanel>
				<com:TCallback ID="VolumeTableElementCall" OnCallback="Page.VolumeWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->Data['mediaid']%>">
					<prop:ClientSide.OnComplete>
						volumeConfigurationWindow.show();
						volumeConfigurationWindow.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->VolumeTableElement->ClientID%>').up('tr').observe('click', function() {
						var request = <%= $this->VolumeTableElementCall->ActiveControl->Javascript %>;
						volumeConfigurationWindow.openConfigurationWindow(request, volumeSlideWindowObj);
					});
				</script>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="slot"
			HeaderText="Slot"
			DataField="slot"
			ItemStyle.HorizontalAlign="Center"
		/>
		<com:TActiveTemplateColumn HeaderText="Pool" SortExpression="pool">
			<prop:ItemTemplate>
				<%=$this->getParent()->Data['pool']['name']%>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn HeaderText="Status" SortExpression="volstatus">
			<prop:ItemTemplate>
				<div id="<%=$this->getParent()->Data['volumename']%>_sizebar" class="status-bar-detail-<%=strtolower($this->getParent()->Data['volstatus'])%>"><%=$this->getParent()->Data['volstatus']%></div>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="whenexpire"
			HeaderText="When expire"
			DataField="whenexpire"
		/>
	</com:TActiveDataGrid>
	</com:TActivePanel>
</com:TContent>
