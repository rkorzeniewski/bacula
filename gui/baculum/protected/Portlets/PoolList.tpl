<%@ MasterClass="Application.Portlets.SlideWindow"%>
<com:TContent ID="SlideWindowContent">
	<script type="text/javascript">
		document.observe("dom:loaded", function() {
			poolConfigurationWindow = ConfigurationWindow<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%>;
			poolSlideWindowObj = <%=$this->getPage()->PoolWindow->ShowID%>SlideWindow;
			poolSlideWindowObj.setConfigurationObj(poolConfigurationWindow);
		});
	</script>
	<com:TActivePanel ID="RepeaterShow">
		<com:TActiveRepeater ID="Repeater">
			<prop:ItemTemplate>
				<com:TPanel ID="PoolElement" CssClass="slide-window-element">
					<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/pool.png" alt="" /><%=@$this->DataItem->name%>
					<input type="hidden" name="<%=$this->ClientID%>" value="<%=isset($this->DataItem->poolid) ? $this->DataItem->poolid : ''%>" />
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
			<com:TActiveTemplateColumn HeaderText="Pool name" SortExpression="name">
				<prop:ItemTemplate>
					<div><%=$this->getParent()->Data['name']%></div>
					<input type="hidden" name="<%=$this->getParent()->ClientID%>" value="<%=$this->getParent()->Data['poolid']%>" />
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveBoundColumn
				SortExpression="numvols"
				HeaderText="Vol. number"
				DataField="numvols"
				ItemStyle.HorizontalAlign="Center"
			/>
			<com:TActiveTemplateColumn HeaderText="Vol. retention" SortExpression="volretention">
				<prop:ItemTemplate>
					<%=(integer)($this->getParent()->Data['volretention'] / 3600 / 24)%> <%=$this->getParent()->Data['volretention'] < 172800 ? 'day' : 'days'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="AutoPrune" SortExpression="autoprune" ItemStyle.HorizontalAlign="Center">
				<prop:ItemTemplate>
					<%=$this->getParent()->Data['autoprune'] == 1 ? 'Yes' : 'No'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
			<com:TActiveTemplateColumn HeaderText="Recycle" SortExpression="recycle" ItemStyle.HorizontalAlign="Center">
				<prop:ItemTemplate>
					<%=$this->getParent()->Data['recycle'] == 1 ? 'Yes' : 'No'%>
				</prop:ItemTemplate>
			</com:TActiveTemplateColumn>
		</com:TActiveDataGrid>
	</com:TActivePanel>
	<com:TCallback ID="DataElementCall" OnCallback="Page.PoolWindow.configure">
		<prop:ClientSide OnComplete="poolConfigurationWindow.show();poolConfigurationWindow.progress(false);" />
	</com:TCallback>
</com:TContent>
