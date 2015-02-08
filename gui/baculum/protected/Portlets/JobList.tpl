<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<com:TPanel ID="JobElement" CssClass="slide-window-element">
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/job-icon.png" alt="" /> [<%=@$this->DataItem->jobid%>] <%=@$this->DataItem->name%>
				<input type="hidden" name="<%=$this->ClientID%>" value="<%=isset($this->DataItem->jobid) ? $this->DataItem->jobid : ''%>" />
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
		<com:TActiveTemplateColumn HeaderText="<input type='checkbox' name='actions_checkbox' onclick=SlideWindow.getObj('JobWindow').markAllChecked(this.checked)>" ItemStyle.HorizontalAlign="Center">
			<prop:ItemTemplate>
				<input type="checkbox" name="actions_checkbox" value="<%=$this->getParent()->Data['jobid']%>" id="<%=$this->getPage()->JobWindow->CheckedValues->ClientID%><%=$this->getParent()->Data['jobid']%>" rel="<%=$this->getPage()->JobWindow->CheckedValues->ClientID%>" onclick="SlideWindow.getObj('JobWindow').markChecked(this.getAttribute('rel'), this.checked, this.value, true);" />
			</prop:ItemTemplate>
                </com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="jobid"
			HeaderText="ID"
			DataField="jobid"
			ItemStyle.HorizontalAlign="Center"
		/>
		<com:TActiveTemplateColumn HeaderText="<%[ Job name ]%>" SortExpression="name">
			<prop:ItemTemplate>
				<div><%=$this->getParent()->Data['name']%></div>
                                <input type="hidden" name="<%=$this->getParent()->ClientID%>" value="<%=$this->getParent()->Data['jobid']%>" />
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn ItemTemplate="<%=isset($this->getPage()->JobWindow->jobTypes[$this->getParent()->Data['type']]) ? $this->getPage()->JobWindow->jobTypes[$this->getParent()->Data['type']] : ''%>" SortExpression="type">
			<prop:HeaderText>
				<span title="<%=Prado::localize('Type')%>" style="cursor: help">T</span>
			</prop:HeaderText>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn ItemTemplate="<%=array_key_exists($this->getParent()->Data['level'], $this->getPage()->JobWindow->jobLevels) ? $this->getPage()->JobWindow->jobLevels[$this->getParent()->Data['level']] : ''%>" SortExpression="level">
			<prop:HeaderText>
				<span title="<%=Prado::localize('Level')%>" style="cursor: help">L</span>
			</prop:HeaderText>
		</com:TActiveTemplateColumn>
		<com:TActiveTemplateColumn HeaderText="<%[ Job status ]%>" SortExpression="jobstatus">
			<prop:ItemTemplate>
				<div class="job-status-<%=isset($this->getParent()->Data['jobstatus']) ? $this->getParent()->Data['jobstatus'] : ''%>" title="<%=isset($this->getPage()->JobWindow->jobStates[$this->getParent()->Data['jobstatus']]['description']) ? $this->getPage()->JobWindow->jobStates[$this->getParent()->Data['jobstatus']]['description'] : ''%>"><%=isset($this->getPage()->JobWindow->jobStates[$this->getParent()->Data['jobstatus']]['value']) ? $this->getPage()->JobWindow->jobStates[$this->getParent()->Data['jobstatus']]['value'] : ''%></div>
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
			SortExpression="endtime"
			HeaderText="<%[ End time ]%>"
			DataField="endtime"
		/>
	</com:TActiveDataGrid>
	<com:TActiveHiddenField ID="CheckedValues" />
	</com:TActivePanel>
	<com:TCallback ID="DataElementCall" OnCallback="Page.JobWindow.configure">
		<prop:ClientSide.OnComplete>
			ConfigurationWindow.getObj('JobWindow').show();
			ConfigurationWindow.getObj('JobWindow').progress(false);
		</prop:ClientSide.OnComplete>
	</com:TCallback>
</com:TContent>
