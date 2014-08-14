<%@ MasterClass="Application.Portlets.SlideWindow"%>
<com:TContent ID="SlideWindowContent">
	<com:TActivePanel ID="RepeaterShow">
		<script type="text/javascript">
			document.observe("dom:loaded", function() {
				poolConfigurationWindow = ConfigurationWindow<%=$this->getPage()->PoolConfiguration->getMaster()->ClientID%>;
				poolSlideWindowObj = <%=$this->getPage()->PoolWindow->ShowID%>SlideWindow;
			});
		</script>
		<com:TActiveRepeater ID="Repeater">
			<prop:ItemTemplate>
				<com:TPanel ID="PoolElement" CssClass="slide-window-element">
					<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/pool.png" alt="" /><%=@$this->DataItem->name%>
				</com:TPanel>
				<com:TCallback ID="PoolElementCall" OnCallback="Page.PoolWindow.configure" ActiveControl.CallbackParameter="<%=@$this->DataItem->poolid%>">
					<prop:ClientSide.OnComplete>
						poolConfigurationWindow.show();
						poolConfigurationWindow.progress(false);
					</prop:ClientSide.OnComplete>
				</com:TCallback>
				<script type="text/javascript">
					$('<%=$this->PoolElement->ClientID%>').observe('click', function() {
						var request = <%= $this->PoolElementCall->ActiveControl->Javascript %>;
						poolConfigurationWindow.openConfigurationWindow(request, poolSlideWindowObj);
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
			<com:TActiveTemplateColumn HeaderText="Pool name" SortExpression="name">
				<prop:ItemTemplate>
					<com:TPanel ID="PoolTableElement"><%=$this->getParent()->Data['name']%></com:TPanel>
					<com:TCallback ID="PoolTableElementCall" OnCallback="Page.PoolWindow.configure" ActiveControl.CallbackParameter="<%=$this->getParent()->Data['poolid']%>">
						<prop:ClientSide.OnComplete>
							poolConfigurationWindow.show();
							poolConfigurationWindow.progress(false);
						</prop:ClientSide.OnComplete>
					</com:TCallback>
					<script type="text/javascript">
						$('<%=$this->PoolTableElement->ClientID%>').up('tr').observe('click', function() {
							var request = <%= $this->PoolTableElementCall->ActiveControl->Javascript %>;
							poolConfigurationWindow.openConfigurationWindow(request, poolSlideWindowObj);
						});
					</script>
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
</com:TContent>
