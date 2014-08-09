<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
		<com:TActiveRepeater ID="Repeater">
			<prop:ItemTemplate>
				<com:TPanel ID="StorageElement" CssClass="slide-window-element">
					<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/server-storage-icon.png" alt="" /><%=@$this->DataItem->name%>
				</com:TPanel>
				<com:TCallback ID="StorageElementCall" OnCallback="Page.StorageWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem->storageid%>">
					<prop:ClientSide.OnComplete>
						ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.show();
						ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->StorageElement->ClientID%>').observe('click', function() {
						var request = <%= $this->StorageElementCall->ActiveControl->Javascript %>;
						if(ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.is_progress() == false) {
							ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.progress(true);
							if(<%=$this->getPage()->StorageWindow->ShowID%>SlideWindow.isFullSize()) {
								<%=$this->getPage()->StorageWindow->ShowID%>SlideWindow.resetSize();
							}
							request.dispatch();
						}
					});
				</script>
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
			<com:TActiveTemplateColumn HeaderText="Storage name" SortExpression="name">
				<prop:ItemTemplate>
					<com:TPanel ID="StorageTableElement"><%=$this->getParent()->Data['name']%></com:TPanel>
					<com:TCallback ID="StorageTableElementCall" OnCallback="Page.StorageWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->Data['storageid']%>">
						<prop:ClientSide.OnComplete>
							ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.show();
							ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.progress(false);
						</prop:ClientSide.OnComplete>
					</com:TCallback>
					<script type="text/javascript">
						$('<%=$this->StorageTableElement->ClientID%>').up('tr').observe('click', function() {
							var request = <%= $this->StorageTableElementCall->ActiveControl->Javascript %>;
							if(ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.is_progress() == false) {
								ConfigurationWindow<%=$this->getPage()->StorageConfiguration->getMaster()->ClientID%>.progress(true);
								if(<%=$this->getPage()->StorageWindow->ShowID%>SlideWindow.isFullSize()) {
									<%=$this->getPage()->StorageWindow->ShowID%>SlideWindow.resetSize();
								}
								request.dispatch();
							}
						});
					</script>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="Autochanger" SortExpression="autochanger" ItemStyle.HorizontalAlign="Center">
				<prop:ItemTemplate>
					<%=$this->getParent()->Data['autochanger'] == 1 ? 'Yes' : 'No'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
		</com:TActiveDataGrid>
	</com:TActivePanel>
</com:TContent>
