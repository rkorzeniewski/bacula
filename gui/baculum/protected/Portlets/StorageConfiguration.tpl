<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
		<strong><%[ Storage name: ]%> <com:TActiveLabel ID="StorageName" /><com:TActiveLabel ID="StorageID" Visible="false" /></strong>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="AutoChangerGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full">
			<com:TActiveTextBox ID="ShowStorage" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 190px" ReadOnly="true" />
		</div>
		<com:TActivePanel ID="AutoChanger" Visible="false" Style="margin-bottom: 10px">
			<div class="line">
				<div class="text"><com:TLabel ForControl="Drive" Text="<%[ Drive number: ]%>" /></div>
				<div class="field">
					<com:TActiveTextBox ID="Drive" AutoPostBack="false" Text="0" MaxLength="3" CssClass="textbox-short" />
					<com:TActiveCustomValidator ID="DriveValidator" ValidationGroup="AutoChangerGroup" ControlToValidate="Drive" ErrorMessage="<%[ Drive number must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="driveValidator" />
				</div>
			</div>
			<div class="line">
				<div class="text"><com:TLabel ForControl="Slot" Text="<%[ Slot number: ]%>" /></div>
				<div class="field">
					<com:TActiveTextBox ID="Slot" AutoPostBack="false" Text="0" MaxLength="3" CssClass="textbox-short" />
					<com:TActiveCustomValidator ID="SlotValidator" ValidationGroup="AutoChangerGroup" ControlToValidate="Slot" ErrorMessage="<%[ Slot number must be integer. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="slotValidator" />
				</div>
			</div>
		</com:TActivePanel>
		<div class="button-center">
			<com:Application.Portlets.BActiveButton ID="Mount" ValidationGroup="AutoChangerGroup" CausesValidation="true" Text="<%[ Mount ]%>" />&nbsp;&nbsp;<com:Application.Portlets.BActiveButton ID="Release" Text="<%[ Release ]%>" />&nbsp;&nbsp;<com:Application.Portlets.BActiveButton ID="Umount" ValidationGroup="AutoChangerGroup" CausesValidation="true" Text="<%[ Umount ]%>" />&nbsp;&nbsp;<com:Application.Portlets.BActiveButton ID="Status" Text="<%[ Status ]%>" />
		</div>
</com:TContent>
