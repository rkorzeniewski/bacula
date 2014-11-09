<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<script type="text/javascript">
		document.observe("dom:loaded", function() {
			storageConfigurationWindow = ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>;
			storageSlideWindowObj = <%=$this->getPage()->StorageWindow->ShowID%>SlideWindow;
			storageSlideWindowObj.setConfigurationObj(storageConfigurationWindow);
		});
	</script>
	<com:TActivePanel ID="RepeaterShow">
		<com:TActiveRepeater ID="Repeater">
			<prop:ItemTemplate>
				<com:TPanel ID="StorageElement" CssClass="slide-window-element">
					<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/server-storage-icon.png" alt="" /><%=@$this->DataItem->name%>
					<input type="hidden" name="<%=$this->ClientID%>" value="<%=isset($this->DataItem->storageid) ? $this->DataItem->storageid : ''%>" />
				</com:TPanel>
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
			<com:TActiveTemplateColumn HeaderText="Storage name" SortExpression="name">
				<prop:ItemTemplate>
					<div><%=$this->getParent()->Data['name']%></div>
					<input type="hidden" name="<%=$this->getParent()->ClientID%>" value="<%=$this->getParent()->Data['storageid']%>" />
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="Autochanger" SortExpression="autochanger" ItemStyle.HorizontalAlign="Center">
				<prop:ItemTemplate>
					<%=$this->getParent()->Data['autochanger'] == 1 ? 'Yes' : 'No'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
		</com:TActiveDataGrid>
	</com:TActivePanel>
	<com:TCallback ID="DataElementCall" OnCallback="Page.StorageWindow.configure">
		<prop:ClientSide OnComplete="storageConfigurationWindow.show();storageConfigurationWindow.progress(false);" />
	</com:TCallback>
</com:TContent>
