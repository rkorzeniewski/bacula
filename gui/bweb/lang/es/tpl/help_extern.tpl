<br/>
<div class='titlediv'>
 <h1 class='newstitle'>Ayuda para expulsar medios (part 1/2)</h1>
</div>
<div class='bodydiv'>
Se seleccionará el mejor candidato para expulsar. Se le pedirá realizar su selección en la próxima pantalla.
  <form action="?" method='GET'>
   <table>
    <tr><td>Pool:</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP db_pools>
             <option selected><TMPL_VAR NAME=name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Tipo Medio:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP db_mediatypes>
             <option><TMPL_VAR mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td> Ubicación : </td>
        <td><select name='location' class='formulaire'>
  <TMPL_LOOP db_locations>
      <option id='loc_<TMPL_VAR location>' value='<TMPL_VAR location>'><TMPL_VAR location></option>
  </TMPL_LOOP>
    </select>
        </td>
    </tr>
    <tr>
        <td>Number of media <br/> to eject:</td> 
        <td> <input type='text' name='limit' size='3' class='formulaire' 
              value='10'> </td>
    </tr>
    <tr>
        <td><button type="submit" class="bp" name='action' value='compute_extern_media' title='Siguiente'> <img src='/bweb/next.png' alt=''>Siguiente</button>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
