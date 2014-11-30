<%@ MasterClass="Application.Portlets.SlideWindow" %>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
	<com:TActiveRepeater ID="Repeater">
		<prop:ItemTemplate>
			<%=($this->getPage()->JobRunWindow->oldDirector != $this->DataItem['director']) ? '<div class="window-section"><span>' . Prado::localize('Director:') . ' ' . $this->DataItem['director']  . '<span></div>': ''%>
			<com:TPanel ID="JobRunElement" CssClass="slide-window-element" >
				<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/job-icon.png" alt="" /> <%=@$this->DataItem['name']%>
				<input type="hidden" name="<%=$this->ClientID%>" value="<%=isset($this->DataItem['name']) ? $this->DataItem['name'] : ''%>" />

			</com:TPanel>
			<%=!($this->getPage()->JobRunWindow->oldDirector = $this->DataItem['director'])%>
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
		<com:TActiveTemplateColumn HeaderText="<%[ Job name ]%>" SortExpression="name">
			<prop:ItemTemplate>
				<div><%=$this->getParent()->DataItem['name']%></div>
                                <input type="hidden" name="<%=$this->getParent()->ClientID%>" value="<%=$this->getParent()->DataItem['name']%>" />
			</prop:ItemTemplate>
		</com:TActiveTemplateColumn>
		<com:TActiveBoundColumn
				SortExpression="director"
				HeaderText="<%[ Director ]%>"
				DataField="director"
				ItemStyle.HorizontalAlign="Center"
			/>
	</com:TActiveDataGrid>
	</com:TActivePanel>
	<com:TCallback ID="DataElementCall" OnCallback="Page.JobRunWindow.configure">
		<prop:ClientSide.OnComplete>
			ConfigurationWindow.getObj('JobRunWindow').show();
			ConfigurationWindow.getObj('JobRunWindow').progress(false);
		</prop:ClientSide.OnComplete>
	</com:TCallback>
</com:TContent>
