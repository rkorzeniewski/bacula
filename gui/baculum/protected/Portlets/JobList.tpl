<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<com:TPanel ID="JobElement" CssClass="slide-window-element">
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/job-icon.png" alt="" /> [<%=@$this->DataItem->jobid%>] <%=@$this->DataItem->name%>
			</com:TPanel>
			<com:TCallback ID="JobElementCall" OnCallback="Page.JobWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem->jobid%>">
				<prop:ClientSide.OnComplete>
					ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.show();
					ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.progress(false);
				</prop:ClientSide.OnComplete>
			</com:TCallback>
			<script type="text/javascript">
				$('<%=$this->JobElement->ClientID%>').observe('click', function() {
					var request = <%= $this->JobElementCall->ActiveControl->Javascript %>;
					if(ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.is_progress() == false) {
						ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.progress(true);
						if(<%=$this->getPage()->JobWindow->ShowID%>SlideWindow.isFullSize()) {
							<%=$this->getPage()->JobWindow->ShowID%>SlideWindow.resetSize();
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
		<com:TActiveBoundColumn
			SortExpression="jobid"
			HeaderText="ID"
			DataField="jobid"
			ItemStyle.HorizontalAlign="Center"
		/>
		<com:TActiveTemplateColumn HeaderText="Job name" SortExpression="name">
			<prop:ItemTemplate>
				<com:TPanel ID="JobTableElement"><%=$this->getParent()->Data['name']%></com:TPanel>
				<com:TCallback ID="JobTableElementCall" OnCallback="Page.JobWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->Data['jobid']%>">
					<prop:ClientSide.OnComplete>
						ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.show();
						ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->JobTableElement->ClientID%>').up('tr').observe('click', function() {
						var request = <%= $this->JobTableElementCall->ActiveControl->Javascript %>;
						if(ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.is_progress() == false) {
							ConfigurationWindow<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>.progress(true);
							if(<%=$this->getPage()->JobWindow->ShowID%>SlideWindow.isFullSize()) {
								<%=$this->getPage()->JobWindow->ShowID%>SlideWindow.resetSize();
							}
							request.dispatch();
						}
					});
				</script>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn ItemTemplate="<%=$this->getPage()->JobWindow->getJobType($this->getParent()->Data['type'])%>" SortExpression="type">
			<prop:HeaderText>
				<span title="Type" style="cursor: help">T</span>
			</prop:HeaderText>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn ItemTemplate="<%=array_key_exists($this->getParent()->Data['level'], $this->getPage()->JobWindow->jobLevels) ? mb_substr($this->getPage()->JobWindow->jobLevels[$this->getParent()->Data['level']], 0, 4) : ''%>" SortExpression="level">
			<prop:HeaderText>
				<span title="Level" style="cursor: help">L</span>
			</prop:HeaderText>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn HeaderText="Job status" SortExpression="jobstatus">
			<prop:ItemTemplate>
				<div class="job-status-<%=$this->getParent()->Data['jobstatus']%>" title="<%=isset($this->getPage()->JobWindow->getJobState($this->getParent()->Data['jobstatus'])->description) ? $this->getPage()->JobWindow->getJobState($this->getParent()->Data['jobstatus'])->description : ''%>"><%=isset($this->getPage()->JobWindow->getJobState($this->getParent()->Data['jobstatus'])->value) ? $this->getPage()->JobWindow->getJobState($this->getParent()->Data['jobstatus'])->value : ''%></div>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="endtime"
			HeaderText="End time"
			DataField="endtime"
		/>
	</com:TActiveDataGrid>
	</com:TActivePanel>
</com:TContent>
