<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
		<script type="text/javascript">
			document.observe("dom:loaded", function() {
				jobRunConfigurationWindow = ConfigurationWindow<%=$this->getPage()->JobRunConfiguration->getMaster()->ClientID%>;
				jobRunSlideWindowObj = <%=$this->getPage()->JobRunWindow->ShowID%>SlideWindow;
			});
		</script>
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<%=($this->getPage()->JobRunWindow->oldDirector != $this->DataItem['director']) ? '<div class="window-section"><span>Director: ' . $this->DataItem['director']  . '<span></div>': ''%>
			<com:TPanel ID="JobRunElement" CssClass="slide-window-element" >
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/job-icon.png" alt="" /> <%=@$this->DataItem['name']%>
			</com:TPanel>
			<com:TCallback ID="JobRunElementCall" OnCallback="Page.JobRunWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem['name']%>">
				<prop:ClientSide.OnComplete>
					jobRunConfigurationWindow.show();
					jobRunConfigurationWindow.progress(false);
				</prop:ClientSide.OnComplete>
			</com:TCallback>
			<script type="text/javascript">
				$('<%=$this->JobRunElement->ClientID%>').observe('click', function() {
					var request = <%= $this->JobRunElementCall->ActiveControl->Javascript %>;
					jobRunConfigurationWindow.openConfigurationWindow(request, jobRunSlideWindowObj);
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
				<com:TPanel ID="JobRunTableElement"><%=$this->getParent()->DataItem['name']%></com:TPanel>
				<com:TCallback ID="JobRunTableElementCall" OnCallback="Page.JobRunWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->DataItem['name']%>">
					<prop:ClientSide.OnComplete>
						jobRunConfigurationWindow.show();
						jobRunConfigurationWindow.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->JobRunTableElement->ClientID%>').up('tr').observe('click', function() {
						var request = <%= $this->JobRunTableElementCall->ActiveControl->Javascript %>;
						jobRunConfigurationWindow.openConfigurationWindow(request, jobRunSlideWindowObj);
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
