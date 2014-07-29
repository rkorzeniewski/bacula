<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<%=($this->getPage()->JobRunWindow->oldDirector != $this->DataItem['director']) ? '<div class="window-section"><span>Director: ' . $this->DataItem['director']  . '<span></div>': ''%>
			<com:TPanel ID="JobElement" CssClass="slide-window-element" >
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/job-icon.png" alt="" /> <%=@$this->DataItem['name']%>
			</com:TPanel>
			<com:TCallback ID="JobElementCall" OnCallback="Page.JobRunWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem['name']%>">
				<prop:ClientSide.OnComplete>
					ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.show();
					ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.progress(false);
					if(<%=$this->getPage()->JobRunWindow->ShowID%>SlideWindow.isFullSize()) {
						<%=$this->getPage()->JobRunWindow->ShowID%>SlideWindow.resetSize();
					}
				</prop:ClientSide.OnComplete>
			</com:TCallback>
			<script type="text/javascript">
				$('<%=$this->JobElement->ClientID%>').observe('click', function() {
					var request = <%= $this->JobElementCall->ActiveControl->Javascript %>;
					if(ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.is_progress() == false) {
						ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.progress(true);
						request.dispatch();
					}
				});
			</script>
			<%=!($this->getPage()->JobRunWindow->oldDirector = $this->DataItem['director'])%>
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
		<com:TActiveTemplateColumn HeaderText="Job name" SortExpression="name">
			<prop:ItemTemplate>
				<com:TPanel ID="JobTableElement"><%=$this->getParent()->DataItem['name']%></com:TPanel>
				<com:TCallback ID="JobTableElementCall" OnCallback="Page.JobRunWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->DataItem['name']%>">
					<prop:ClientSide.OnComplete>
						ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.show();
						ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.progress(false);
						if(<%=$this->getPage()->JobRunWindow->ShowID%>SlideWindow.isFullSize()) {
							<%=$this->getPage()->JobRunWindow->ShowID%>SlideWindow.resetSize();
						}
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->JobTableElement->ClientID%>').up('tr').observe('click', function() {
						var request = <%= $this->JobTableElementCall->ActiveControl->Javascript %>;
						if(ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.is_progress() == false) {
							ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>.progress(true);
							request.dispatch();
						}
					});
				</script>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
				SortExpression="director"
				HeaderText="Director"
				DataField="director"
				ItemStyle.HorizontalAlign="Center"
			/>
	</com:TActiveDataGrid>
	</com:TActivePanel>
</com:TContent>
