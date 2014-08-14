<%@ MasterClass="Application.Portlets.SlideWindow"%>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
		<script type="text/javascript">
			document.observe("dom:loaded", function() {
				clientConfigurationWindow = ConfigurationWindow<%=$this->getPage()->ClientConfiguration->getMaster()->ClientID%>;
				clientSlideWindowObj = <%=$this->getPage()->ClientWindow->ShowID%>SlideWindow;
			});
		</script>
		<com:TActiveRepeater ID="Repeater">
			<prop:ItemTemplate>
				<com:TPanel ID="ClientElement" CssClass="slide-window-element" ToolTip="<%=@$this->DataItem['uname']%>">
					<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/client-icon.png" alt="" /><%=@$this->DataItem['name']%>
				</com:TPanel>
				<com:TCallback ID="ClientElementCall" OnCallback="Page.ClientWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem['clientid']%>">
					<prop:ClientSide.OnComplete>
						clientConfigurationWindow.show();
						clientConfigurationWindow.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->ClientElement->ClientID%>').observe('click', function() {
						var request = <%= $this->ClientElementCall->ActiveControl->Javascript %>;
						clientConfigurationWindow.openConfigurationWindow(request, clientSlideWindowObj);
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
			<com:TActiveTemplateColumn HeaderText="Client name" SortExpression="name">
				<prop:ItemTemplate>
					<com:TPanel ID="ClientTableElement"><%=$this->getParent()->Data['name']%></com:TPanel>
					<com:TCallback ID="ClientTableElementCall" OnCallback="Page.ClientWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->Data['clientid']%>">
						<prop:ClientSide.OnComplete>
							clientConfigurationWindow.show();
							clientConfigurationWindow.progress(false);
						</prop:ClientSide.OnComplete>
					</com:TCallback>
					<script type="text/javascript">
						$('<%=$this->ClientTableElement->ClientID%>').up('tr').observe('click', function() {
							var request = <%= $this->ClientTableElementCall->ActiveControl->Javascript %>;
							clientConfigurationWindow.openConfigurationWindow(request, clientSlideWindowObj);
						});
					</script>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn ItemStyle.HorizontalAlign="Center" HeaderText="AutoPrune" SortExpression="autoprune">
				<prop:ItemTemplate>
					<%=$this->getParent()->Data['autoprune'] == 1 ? 'Yes' : 'No'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="File Retention" SortExpression="fileretention">
				<prop:ItemTemplate>
					<%=(integer)($this->getParent()->Data['fileretention'] / 3600 / 24)%> <%=$this->getParent()->Data['fileretention'] < 172800 ? 'day' : 'days'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="Job Retention" SortExpression="jobretention">
				<prop:ItemTemplate>
					<%=(integer)($this->getParent()->Data['jobretention'] / 3600 / 24)%> <%=$this->getParent()->Data['jobretention'] < 172800 ? 'day' : 'days'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
		</com:TActiveDataGrid>
	</com:TActivePanel>
</com:TContent>
